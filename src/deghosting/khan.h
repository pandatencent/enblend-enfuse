
#ifndef KHAN_H_
#define KHAN_H_

#include "deghosting.h"
// for AlgTinyVector, NormalizeFunctor and LogarithmFunctor
#include "support.h"

// needed for RGB2Lab
#include <vigra/imageinfo.hxx>
#include <vigra/transformimage.hxx>
#include <vigra/colorconversions.hxx>

// for RGBvalue used in hat function
#include <vigra/rgbvalue.hxx>

// needed for Kh()
#define PI 3.14159265358979323846

// number of pixels to look at in all directions
// ie. 1 for neighbourhood of size 3x3, 2 for 5x5 etc.
#define NEIGHB_DIST 1

// define for use atan based kernel function
// leave undefined for gaussian normal distribution function
//#define ATAN_KH

#ifdef DEGHOSTING_CACHE_IMAGES
    #include <cstring>
    #include <vigra/cachedfileimage.hxx>
#endif

using namespace vigra;

namespace deghosting
{
    #ifdef DEGHOSTING_CACHE_IMAGES
        typedef CachedFileImage<AlgTinyVector<float, 3> > FLabImage;
        typedef boost::shared_ptr<FLabImage> FLabImagePtr;
    #else
        typedef BasicImage<AlgTinyVector<float, 3> > FLabImage;
        typedef boost::shared_ptr<FLabImage> FLabImagePtr;
    #endif
    
    class Khan : public Deghosting
    {
        public:
            Khan(std::vector<std::string>& inputFiles, const uint16_t flags, const uint16_t debugFlags, int iterations, double sigma, int verbosity);
            Khan(std::vector<ImageImportInfo>& inputFiles, const uint16_t flags, const uint16_t debugFlags, int iterations, double sigma, int verbosity);
            std::vector<FImagePtr> createWeightMasks();
            ~Khan() {}
        protected:
            // Kh() things
            // (2*pi)^(1/2)
            double PIPOW;
            // 1/Kh denominator
            double denom;
            // sigma in gauusian density function
            double sigma;
            
            // other necessary stuff
            std::vector<FLabImagePtr> LabImages;
            std::vector<FImagePtr> weights;
            
            /** hat function
             * used for creating initial weights
             */
            static inline float hat(RGBValue<float> pixel);
            
            /** set sigma
             * sets sigma for gaussian weigting function
             */
            void setSigma(double sigma);
            
            /** transform image using EMoR response
             * @param inputFile filename of image to be transformed
             * @param *pInputImg FRGBImage to be transformed
             */
            //void linearizeRGB(std::string, FRGBImage* pInputImg);
            
            /** kernel function
             * Standard probability density function
             */
            inline double Kh(deghosting::AlgTinyVector< float, 3 > x);
            
            /** function to preprocess input image
             * This function loads image, linearize it using EMoR (FIXME),
             * tranform it using logarithm or gamma if input images are HDR
             */
            void preprocessImage(unsigned int i, FImagePtr &weight, FLabImagePtr &LabImage);
    };
}

#endif /* KHAN_H_ */