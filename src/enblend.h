/*
 * Copyright (C) 2004 Andrew Mihal
 *
 * This file is part of Enblend.
 *
 * Enblend is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Enblend is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Enblend; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __ENBLEND_H__
#define __ENBLEND_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//#include <boost/static_assert.hpp>
#include <iostream>
#include <list>
#include <stdio.h>

#include "assemble.h"
#include "bounds.h"
#include "mask.h"
#include "pyramid.h"

#include "common.h"
#include "vigra/impex.hxx"
#include "vigra/initimage.hxx"

using std::cout;
using std::endl;
using std::list;
using std::pair;

using vigra::BasicImage;
using vigra::BImage;
using vigra::ImageExportInfo;
using vigra::ImageImportInfo;
using vigra::initImageIf;
using vigra::NumericTraits;
using vigra::VigraTrueType;
using vigra::VigraFalseType;

namespace enblend {

template <typename ImageType, typename MaskPyramidType, typename ImagePyramidType>
void enblendMain(list<ImageImportInfo*> &imageInfoList,
        ImageExportInfo &outputImageInfo,
        EnblendROI &inputUnion) {

    typedef typename ImageType::value_type ImageValueType;
    typedef BImage AlphaType;
    typedef typename AlphaType::value_type AlphaValueType;
    typedef typename MaskPyramidType::value_type MaskPyramidValueType;
    typedef typename ImagePyramidType::value_type ImagePyramidValueType;

    cout << "sizeof(ImageValueType) = " << sizeof(ImageValueType) << endl;
    cout << "sizeof(AlphaValueType) = " << sizeof(AlphaValueType) << endl;
    cout << "sizeof(MaskPyramidValueType) = " << sizeof(MaskPyramidValueType) << endl;
    cout << "sizeof(ImagePyramidValueType) = " << sizeof(ImagePyramidValueType) << endl;

    // Create the initial black image.
    EnblendROI blackBB;
    pair<ImageType*, AlphaType*> blackPair =
            assemble<ImageType, AlphaType>(imageInfoList, inputUnion, blackBB);
    exportImageAlpha(srcImageRange(*(blackPair.first)),
                     srcImage(*(blackPair.second)),
                     outputImageInfo);
    // mem xsection = up to 2*inputUnion*ImageValueType

    // Main blending loop.
    while (!imageInfoList.empty()) {

        // Create the white image.
        EnblendROI whiteBB;
        pair<ImageType*, AlphaType*> whitePair =
                assemble<ImageType, AlphaType>(imageInfoList, inputUnion, whiteBB);
        ImageExportInfo whiteInfo("enblend_white.tif");
        exportImageAlpha(srcImageRange(*(whitePair.first)),
                         srcImage(*(whitePair.second)),
                         whiteInfo);
        // mem xsection = up to 2*inputUnion*ImageValueType

        // Union bounding box of whiteImage and blackImage.
        EnblendROI uBB;
        whiteBB.unite(blackBB, uBB);

        // Intersection bounding box of whiteImage and blackImage.
        EnblendROI iBB;
        bool overlap = whiteBB.intersect(blackBB, iBB);

        // Calculate ROI bounds and number of levels from iBB.
        // ROI bounds not to extend uBB.
        // FIXME consider case where overlap==false
        EnblendROI roiBB;
        unsigned int numLevels = roiBounds<MaskPyramidValueType>(inputUnion, iBB, uBB, roiBB);
        bool wraparoundThisIteration = Wraparound && (roiBB.size().x == inputUnion.size().x);

        // Create a version of roiBB relative to uBB upperleft corner.
        // This is to access roi within images of size uBB.
        // For example, the mask.
        EnblendROI roiBB_uBB;
        roiBB_uBB.setCorners(roiBB.getUL() - uBB.getUL(), roiBB.getLR() - uBB.getUL());

        // Create the blend mask.
        BImage *mask = createMask<AlphaType, BImage>(whitePair.second, blackPair.second,
                uBB, wraparoundThisIteration);
        ImageExportInfo maskInfo("enblend_mask.tif");
        maskInfo.setPosition(uBB.getUL());
        exportImage(srcImageRange(*mask), maskInfo);

        // Build Gaussian pyramid from mask.
        //vector<int*> *maskGP = gaussianPyramid(numLevels,
        //        mask->upperLeft() + (roiBB.getUL() - uBB.getUL()),
        //        mask->upperLeft() + (roiBB.getLR() - uBB.getUL()),
        //        mask->accessor());
        vector<MaskPyramidType*> *maskGP = gaussianPyramid<BImage, MaskPyramidType>(
                numLevels, wraparoundThisIteration, roiBB_uBB.apply(srcImageRange(*mask)));

        for (unsigned int i = 0; i < numLevels; i++) {
            char filenameBuf[512];
            snprintf(filenameBuf, 512, "enblend_mask_gp%04u.tif", i);
            cout << filenameBuf << endl;
            ImageExportInfo mgpInfo(filenameBuf);
            exportImage(srcImageRange(*((*maskGP)[i])), mgpInfo);
        }

        // Now it is safe to make changes to mask image.
        // Black out the ROI in the mask.
        // Make an roiBounds relative to uBB origin.
        initImage(roiBB_uBB.apply(destImageRange(*mask)),
                NumericTraits<MaskPyramidValueType>::zero());

        // Copy pixels inside whiteBB and inside white part of mask into black image.
        // These are pixels where the white image contributes outside of the ROI.
        // We cannot modify black image inside the ROI yet because we haven't built the
        // black pyramid.
        copyImageIf(uBB.apply(srcImageRange(*(whitePair.first))),
                    maskImage(*mask),
                    uBB.apply(destImage(*(blackPair.first))));

        // We no longer need the mask.
        delete mask;

        // Build Laplacian pyramid from white image.
        //vector<PyramidType*> *whiteLP = whitePair.first, whitePair.second

        // We no longer need the white rgb data.
        delete whitePair.first;

        // Build Laplacian pyramid from black image.

        // Make the black image alpha equal to the union of the
        // white and black alpha channels.
        initImageIf(whiteBB.apply(destImageRange(*(blackPair.second))),
                whiteBB.apply(maskImage(*(whitePair.second))),
                NumericTraits<AlphaValueType>::max());

        // We no longer need the white alpha data.
        delete whitePair.second;

        // Blend pyramids
        // delete mask pyramid
        // delete white pyramid
        // collapse black pyramid

        // copy collapsed black pyramid into black image ROI, using black alpha mask.

        // delete black pyramid

        // Checkpoint results.
        exportImageAlpha(srcImageRange(*(blackPair.first)),
                         srcImage(*(blackPair.second)),
                         outputImageInfo);

        // Now set blackBB to uBB.
        blackBB = uBB;
    }

    delete blackPair.first;
    delete blackPair.second;

};

template <typename ImageType, typename ImagePyramidType>
void enblendMain(list<ImageImportInfo*> &imageInfoList,
        ImageExportInfo &outputImageInfo,
        EnblendROI &inputUnion,
        VigraTrueType) {

    // ImagePyramidType::value_type is a scalar.
    //typedef BasicImage<typename ImagePyramidType::value_type> MaskPyramidType;

    enblendMain<ImageType, ImagePyramidType, ImagePyramidType>(
            imageInfoList, outputImageInfo, inputUnion);
}

template <typename ImageType, typename ImagePyramidType>
void enblendMain(list<ImageImportInfo*> &imageInfoList,
        ImageExportInfo &outputImageInfo,
        EnblendROI &inputUnion,
        VigraFalseType) {

    // ImagePyramidType::value_type is a vector.
    typedef typename ImagePyramidType::value_type VectorType;
    typedef BasicImage<typename VectorType::value_type> MaskPyramidType;

    enblendMain<ImageType, MaskPyramidType, ImagePyramidType>(
            imageInfoList, outputImageInfo, inputUnion);
}

template <typename ImageType, typename ImagePyramidType>
void enblendMain(list<ImageImportInfo*> &imageInfoList,
        ImageExportInfo &outputImageInfo,
        EnblendROI &inputUnion) {

    // This indicates if ImagePyramidType is RGB or grayscale.
    typedef typename NumericTraits<typename ImagePyramidType::value_type>::isScalar pyramid_is_scalar;
    typedef typename NumericTraits<typename ImageType::value_type>::isScalar image_is_scalar;

    // If the image is RGB, the pyramid must also be RGB.
    // If the image is scalar, the pyramid must also be scalar.
    BOOST_STATIC_ASSERT(pyramid_is_scalar::asBool == image_is_scalar::asBool);

    enblendMain<ImageType, ImagePyramidType>(
            imageInfoList, outputImageInfo, inputUnion, pyramid_is_scalar());
}

} // namespace enblend

#endif /* __ENBLEND_H__ */
