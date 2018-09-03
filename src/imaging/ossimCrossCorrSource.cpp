//**************************************************************************************************
//
//     OSSIM Open Source Geospatial Data Processing Library
//     See top level LICENSE.txt file for license information
//
//**************************************************************************************************

#include <ossim/imaging/ossimCrossCorrSource.h>
#include <ossim/imaging/ossimImageDataFactory.h>

// Convenience macros:
#define CINFO  ossimNotify(ossimNotifyLevel_INFO)
#define CWARN  ossimNotify(ossimNotifyLevel_WARN)
#define CFATAL ossimNotify(ossimNotifyLevel_FATAL)

static const int DEFAULT_CMP_PATCH_SIZING_FACTOR = 2;
static const double DEFAULT_NOMINAL_POSITION_ERROR = 50; // meters

ossimCrossCorrSource::ossimCrossCorrSource()
{
   // PRIVATE, SHOULD NEVER BE CALLED. WOULD NEED TO CALL INITIALIZE
}

ossimCrossCorrSource::ossimCrossCorrSource(ossimConnectableObject::ConnectableObjectList& inputSources)
: ossimImageCombiner(inputSources)
{
   initialize();
}

void ossimCrossCorrSource::initialize()
{
   static const char* MODULE = "ossimCrossCorrSource::initialize() -- ";

   if ( getNumberOfInputs() < 2)
   {
      CFATAL<<MODULE << "Insufficient number of inputs! Must be two";
      return;
   }

}

ossimRefPtr<ossimImageData> ossimCrossCorrSource::getTile(const ossimIrect& tileRect,
                                                            ossim_uint32 resLevel)
{
   static const char* MODULE = "ossimCrossCorrSource::getTile()  ";
   if(getNumberOfInputs() < 2)
      return 0;

   m_crossCorrelation.clear();

   // Grab the image patches from the images.
   ossimImageSource* imageA = (ossimImageSource*) getInput(0);
   ossimImageSource* imageB = (ossimImageSource*) getInput(1);
   ossimRefPtr<ossimImageData> patchA (imageA->getTile(tileRect, 0));
   ossimRefPtr<ossimImageData> patchB (imageB->getTile(tileRect, 0));
   if (!patchA.valid() || !patchB.valid())
   {
      CWARN << MODULE << "Error getting patch image data." << endl;
      return false;
   }

   // Loop over all pixels in the tiles for each band:
   unsigned int numBands = getNumberOfInputBands();
   unsigned int numPix = tileRect.area();
   for (int band=0; band<numBands; ++band)
   {
      double sumA2=0, sumB2=0, sumAB=0;

      double avgA = patchA->computeAverageBandValue(band);
      double avgB = patchB->computeAverageBandValue(band);
      double* bufA = patchA->getDoubleBuf(band);
      double* bufB = patchB->getDoubleBuf(band);

      for (unsigned int p=0; p<numPix; ++p)
      {
         sumA2 += bufA[p]*bufA[p];
         sumB2 += bufB[p]*bufB[p];
         sumAB += (bufA[p]-avgA)*(bufB[p]-avgB);
      }

      double crossCorr = sumAB / sqrt(sumA2*sumB2);
      m_crossCorrelation.emplace_back(crossCorr);
   }

   return 0;
}

