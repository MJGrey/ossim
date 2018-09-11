//**************************************************************************************************
//
//     OSSIM Open Source Geospatial Data Processing Library
//     See top level LICENSE.txt file for license information
//
//**************************************************************************************************

#ifndef ossimEqualityTest_HEADER
#define ossimEqualityTest_HEADER

#include <ossim/util/ossimTool.h>
#include <ossim/imaging/ossimImageHandler.h>
#include <ossim/base/ossimRefPtr.h>
#include <ossim/base/ossimFilename.h>
#include <vector>

/**
 * Tool for testing the equality of two images. The test uses the cross-correlation between two
 * images to determine if they are sufficiently equal to pass a comparison test. This is in lieu
 * of pixel-to-pixel comparison that often fails due to numerical differences between machines.
 */
class OSSIMDLLEXPORT ossimEqualityTest : public ossimTool
{
public:
   ossimEqualityTest();
   ~ossimEqualityTest() final = default;

   /**
    * Initializes the aurgument parser with expected parameters and options. It does not output
    * anything. To see the usage, the caller will need to do something like:
    *
    *   ap.getApplicationUsage()->write(<ostream>);
    */
   void setUsage(ossimArgumentParser& ap) final;

   /** Initializes from command line arguments.
    * @return FALSE if --help option requested or no params provided, so that derived classes can
    * @note Throws ossimException on error. */
   bool initialize(ossimArgumentParser& ap) final;

   /** NOT USED */
   void initialize(const ossimKeywordlist& kwl) final {}

   /** Writes product to output file. Returns true if successful.
    * @note Throws ossimException on error. */
   bool execute() final;

   ossimString getClassName() const final { return "ossimEqualityTest"; }

   /** Used by ossimToolFactory */
   static const char* DESCRIPTION;

   /** Fetch pass/fail status after execute is called */
   bool testPassed() const { return m_testPassed; }

protected:
   void createInputChain(const ossimFilename& fname, ossim_uint32 entry_index=0);
   bool doCrossCorrelationTest();
   bool doPixelComparisonTest();

   double m_corrThreshold;
   unsigned int m_maxSizeDiff;
   ossimIpt m_tileSize;
   std::vector< ossimRefPtr<ossimImageHandler> > m_images;
   std::vector< ossimFilename > m_imgNames;
   bool m_testPassed;
   ossimRefPtr<ossimImageData> m_tileA;
   ossimRefPtr<ossimImageData> m_tileB;

};

#endif
