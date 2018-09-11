//*****************************************************************************
//
//     OSSIM Open Source Geospatial Data Processing Library
//     See top level LICENSE.txt file for license information
//
//*****************************************************************************

#include <ossim/util/ossimEqualityTest.h>
#include <ossim/base/ossimApplicationUsage.h>
#include <ossim/base/ossimException.h>
#include <ossim/imaging/ossimImageHandlerRegistry.h>
#include <ossim/imaging/ossimImageSourceSequencer.h>

using namespace std;

const char* ossimEqualityTest::DESCRIPTION =
      "Provides pass/fail comparison of two images given acceptance criteria.";

#define CINFO  ossimNotify(ossimNotifyLevel_INFO)
#define CWARN  ossimNotify(ossimNotifyLevel_WARN)
#define CFATAL ossimNotify(ossimNotifyLevel_FATAL)
#define DEBUG_ENABLED true

typedef ossimRefPtr<ossimImageSourceSequencer> SequencerPtrType;

ossimEqualityTest::ossimEqualityTest()
:   m_corrThreshold (1.0),
    m_maxSizeDiff (0),
    m_testPassed (false)
{
}

void ossimEqualityTest::setUsage(ossimArgumentParser& ap)
{
   // Set the general usage:
   ossimApplicationUsage* au = ap.getApplicationUsage();
   ossimString appName = ap.getApplicationName();
   ossimString usageString = appName;
   usageString += " equality [options] <imageA> <imageB> ";
   au->setCommandLineUsage(usageString);

   // Set the command line options:
   au->addCommandLineOption(
         "--size-tolerance <int>", "Max allowed difference in size between images before flagging "
         "difference. Defaults to 0 pixel.");
   au->addCommandLineOption(
         "--threshold <double>", "Normalized correlation threshold for accepting image comparison."
         " Defaults to 1.0 (exact match).");

   // Base class has its own:
   ossimTool::setUsage(ap);
   au->setDescription(DESCRIPTION);
}

bool ossimEqualityTest::initialize(ossimArgumentParser& ap)
{
   ostringstream xmsg;
   xmsg<<"ossimEqualityTest::initialize(ossimArgumentParser) -- ";

   // Base class first:
   if (!ossimTool::initialize(ap))
      return false;
   if (m_helpRequested)
      return true;

   ossimString ts1;
   ossimArgumentParser::ossimParameter sp1(ts1);

   if ( ap.read("--size-tolerance", sp1) )
      m_maxSizeDiff = ts1.toUInt32();

   if ( ap.read("--threshold", sp1) )
   {
      m_corrThreshold = ts1.toDouble();
      if (m_corrThreshold > 1.0)
         m_corrThreshold = 1.0;
      if (m_corrThreshold < 0.0)
         m_corrThreshold = 0.0;
   }

   if ( ap.argc() < 2 )
   {
      xmsg<<"Expecting more arguments.";
      throw ossimException(xmsg.str()); // NOLINT
   }
   else
   {
      m_imgNames.emplace_back(ap[1]);
      m_imgNames.emplace_back(ap[2]);
   }

   return true;
}

void ossimEqualityTest::createInputChain(const ossimFilename& fname, ossim_uint32 entry_index)
{
   ostringstream xmsg;

   // Establish input image handler:
   ossimRefPtr<ossimImageHandler> handler = ossimImageHandlerRegistry::instance()->open(fname);
   if (!handler)
   {
      xmsg<<"ERROR: ossimEqualityTest:"<<__LINE__<<" -- Could not open <"<<fname<<">"<<ends;
      throw ossimException(xmsg.str()); // NOLINT
   }
   if (!handler->setCurrentEntry( entry_index ))
   {
      xmsg<<" ERROR: ossimEqualityTest:"<<__LINE__<<" -- Entry " <<entry_index<< " out of range!"<<endl;
      throw ossimException( xmsg.str() ); // NOLINT
   }

   m_images.push_back(handler);
}

bool ossimEqualityTest::execute()
{
   m_testPassed = false;
   if (m_helpRequested)
      return true;

   // Base class does most work -- Create chains for input sources:
   createInputChain(m_imgNames[0]);
   createInputChain(m_imgNames[1]);

   while (true)
   {
      // Check size for differences:
      ossimIpt sizeA(m_images[0]->getBoundingRect().size());
      ossimIpt sizeB(m_images[1]->getBoundingRect().size());
      ossimIpt dSize(sizeA - sizeB);
      if ((dSize.x > m_maxSizeDiff) || (dSize.y > m_maxSizeDiff))
      {
         CINFO << "ossimEqualityTest: FAILED size-mismatch" << endl;
         m_response = R"({ "test": "failed", "reason": "size-mismatch" })";
         break;
      };

      // Check number of bands:
      unsigned int nBandsA = m_images[0]->getNumberOfOutputBands();
      unsigned int nBandsB = m_images[1]->getNumberOfOutputBands();
      if (nBandsA != nBandsB)
      {
         CINFO << "\nossimEqualityTest: FAILED band-mismatch" << endl;
         m_response = R"({ "test": "failed", "reason": "band-mismatch" })";
         break;
      };

      // Check radiometry:
      ossimScalarType sA = m_images[0]->getOutputScalarType();
      ossimScalarType sB = m_images[1]->getOutputScalarType();
      if (sA != sB)
      {
         CINFO << "\nossimEqualityTest: FAILED scalar-type-mismatch" << endl;
         m_response = R"({ "test": "failed", "reason": "scalar-type-mismatch" })";
         break;
      };

      // All easy tests passed. Need do image processing next. Set up sequencers and loop over tiles:
      SequencerPtrType sequencerA = new ossimImageSourceSequencer(m_images[0].get());
      SequencerPtrType sequencerB = new ossimImageSourceSequencer(m_images[1].get());
      m_testPassed = true;
      m_tileA = sequencerA->getNextTile();
      m_tileB = sequencerB->getNextTile();
      while (m_tileA && m_tileB && m_testPassed)
      {
         if (m_corrThreshold == 1.0)
            doPixelComparisonTest(); // exact match
         else
            doCrossCorrelationTest(); // close match

         m_tileA = sequencerA->getNextTile();
         m_tileB = sequencerB->getNextTile();
      }

      if (m_testPassed)
      {
         CINFO << "\nossimEqualityTest: PASSED" << endl;
         m_response = R"({ "test": "passed" })";
      }
      else
      {
         CINFO << "\nossimEqualityTest: FAILED pixel-mismatch" << endl;
         m_response = R"({ "test": "failed", "reason": "pixel-mismatch" })";
      }
      break;
   }

   return true;
}

bool ossimEqualityTest::doCrossCorrelationTest()
{
   if (DEBUG_ENABLED)
      CINFO << "\nossimEqualityTest::doCrossCorrelationTest() Results for tile:" << endl;

   // Loop over all pixels in the tiles for each band and compute cross-correlation:
   unsigned int numBands = m_tileA->getNumberOfBands();
   unsigned int numPix = m_tileA->getSizePerBand();
   for (int band = 0; (band < numBands) && m_testPassed; ++band)
   {
      double sumA2 = 0, sumB2 = 0, sumAB = 0;
      double avgA = (double) m_tileA->computeAverageBandValue(band);
      double avgB = (double) m_tileB->computeAverageBandValue(band);
      for (unsigned int p = 0; p < numPix; ++p)
      {
         double a = (double) m_tileA->getPix(p, band) - avgA;
         double b = (double) m_tileB->getPix(p, band) - avgB;
         sumA2 += a * a;
         sumB2 += b * b;
         sumAB += a * b;
      }

      double correlation = sumAB / sqrt(sumA2 * sumB2);

      if (DEBUG_ENABLED)
         CINFO << "    correlation[" << band << "] = " << correlation << endl;

      if (correlation < m_corrThreshold)
         m_testPassed = false;
   }
}

bool ossimEqualityTest::doPixelComparisonTest()
{
   // Loop over all pixels in the tiles for each band and compute cross-correlation:
   unsigned int numBands = m_tileA->getNumberOfBands();
   unsigned int numPix = m_tileA->getSizePerBand();
   for (int band = 0; (band < numBands) && m_testPassed; ++band)
   {
      for (unsigned int p = 0; (p<numPix) && m_testPassed; ++p)
      {
         double a = (double) m_tileA->getPix(p, band);
         double b = (double) m_tileB->getPix(p, band);
         if (a != b)
            m_testPassed = false;
      }
   }
}

