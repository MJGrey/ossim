//*****************************************************************************
//
//     OSSIM Open Source Geospatial Data Processing Library
//     See top level LICENSE.txt file for license information
//
//*****************************************************************************

#include <ossim/util/ossimEqualityTest.h>
#include <ossim/base/ossimApplicationUsage.h>
#include <ossim/base/ossimException.h>

using namespace std;

const char* ossimEqualityTest::DESCRIPTION =
      "Provides pass/fail comparison of two images given acceptance criteria.";

#define CINFO  ossimNotify(ossimNotifyLevel_INFO)
#define CWARN  ossimNotify(ossimNotifyLevel_WARN)
#define CFATAL ossimNotify(ossimNotifyLevel_FATAL)
#define DEBUG_ENABLED true

ossimEqualityTest::ossimEqualityTest()
:   m_corrThreshold (0.99),
    m_maxSizeDiff (1),
    m_tileSize (1024)
{
}

void ossimEqualityTest::setUsage(ossimArgumentParser& ap)
{
   // Set the general usage:
   ossimApplicationUsage* au = ap.getApplicationUsage();
   ossimString appName = ap.getApplicationName();
   ossimString usageString = appName;
   usageString += " equalityTest [options] <imageA> <imageB> ";
   au->setCommandLineUsage(usageString);

   // Set the command line options:
   au->addCommandLineOption(
         "--size-tolerance <int>", "Max allowed difference in size between images before flagging "
         "difference. Defaults to 1 pixel.");
   au->addCommandLineOption(
         "--threshold <double>", "Normalized correlation threshold for accepting image comparison.");
   au->addCommandLineOption(
         "--tile-size <n>", "Specifies the square tile size in pixels to use when sampling random "
         "tiles. Defaults to 1024");

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

   if ( ap.read("--tile-size", sp1) )
   {
      unsigned int n = ts1.toUInt32();
      m_tileSize = ossimIpt(n,n);
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

   // Init chain with handler:
   ossimRefPtr<ossimSingleImageChain> chain = new ossimSingleImageChain;
   if (!chain->open(fname))
   {
      xmsg<<"ERROR: ossimEqualityTest ["<<__LINE__<<"] Could not open <"<<fname<<">"<<ends;
      throw ossimException(xmsg.str()); // NOLINT
   }
   ossimImageHandler* handler = chain->getImageHandler().get();
   if (!handler->setCurrentEntry( entry_index ))
   {
      xmsg << " ERROR: ossimEqualityTest ["<<__LINE__<<"] Entry " << entry_index << " out of range!" << std::endl;
      throw ossimException( xmsg.str() ); // NOLINT
   }

   ossimRefPtr<ossimScalarRemapper> remapper = new ossimScalarRemapper();
   chain->add(remapper.get());
   remapper->setOutputScalarType(OSSIM_NORMALIZED_DOUBLE);

   m_imgLayers.push_back(chain);
}

bool ossimEqualityTest::execute()
{
   if (m_helpRequested)
      return true;

   // Base class does most work -- Create chains for input sources:
   createInputChain(m_imgNames[0]);
   createInputChain(m_imgNames[1]);

   // Check size for differences:
   ossimIpt sizeA(m_imgLayers[0]->getBoundingRect().size());
   ossimIpt sizeB(m_imgLayers[1]->getBoundingRect().size());
   ossimIpt dSize(sizeA - sizeB);
   if ((dSize.x > m_maxSizeDiff) || (dSize.y > m_maxSizeDiff))
   {
      m_response = R"({ "test": "failed", "reason": "size-mismatch" })";
      return false;
   };

   // Check number of bands:
   unsigned int nBandsA = m_imgLayers[0]->getNumberOfOutputBands();
   unsigned int nBandsB = m_imgLayers[1]->getNumberOfOutputBands();
   if (nBandsA != nBandsB)
   {
      m_response = R"({ "test": "failed", "reason": "band-mismatch" })";
      return false;
   };

   // Check radiometry:
   ossimScalarType sA = m_imgLayers[0]->getOutputScalarType();
   ossimScalarType sB = m_imgLayers[1]->getOutputScalarType();
   if (sA != sB)
   {
      m_response = R"({ "test": "failed", "reason": "scalar-type-mismatch" })";
      return false;
   };

   // All easy tests passed. Need do image processing next. The result is a list of cross
   // correlation coefficients for each band. In order for the test to pass, all bands must meet
   // threshold criteria:
   std::vector<double> correlations;
   computeCrossCorrelation(correlations);
   for (int b=0; b<nBandsA; ++b)
   {
      if (correlations[b] < m_corrThreshold)
      {
         m_response = R"({ "test": "failed", "reason": "low-correlation" })";
         return false;
      }
   }

   m_response = R"({ "test": "passed" })";
   return true;
}

void ossimEqualityTest::computeCrossCorrelation(std::vector<double>& correlations)
{
   // Set up the cross-correlation
   // computation source:
   ossimRefPtr<ossimCrossCorrSource> crossCorrSource = new ossimCrossCorrSource;
   crossCorrSource->connectMyInputTo(0, m_imgLayers[0].get());
   crossCorrSource->connectMyInputTo(1, m_imgLayers[1].get());
   crossCorrSource->initialize();

   // Determine whether necessary to do sparse sampling or full image correlation:
   ossimIrect imgRect (crossCorrSource->getBoundingRect());
   int nx = imgRect.width()/m_tileSize.x;
   if (nx > 5)
      nx = 5;
   else if (nx < 1)
   {
      nx = 1;
      m_tileSize.x = imgRect.width();
   }
   int ny = imgRect.height()/m_tileSize.y;
   if (ny > 5)
      ny = 5;
   else if (ny < 1)
   {
      ny = 1;
      m_tileSize.y = imgRect.height();
   }
   int xMax = imgRect.width()-m_maxSizeDiff;
   int yMax = imgRect.height()-m_maxSizeDiff;
   int dx = (xMax)/nx;
   int dy = (yMax)/ny;
   int numTilesSampled = 0;
   unsigned int nBands = crossCorrSource->getNumberOfOutputBands();
   for (int b=0; b<nBands; ++b)
      correlations.push_back(0.0);

   // Now loop over sampling tiles, accumulating the correlation coefficient for each:
   ossimIpt tileOrigin (0,0);
   for (; tileOrigin.y<yMax; tileOrigin.y+=dy)
   {
      for (; tileOrigin.x<xMax; tileOrigin.x+=dx)
      {
         ossimIrect tileRect (tileOrigin, (unsigned int) m_tileSize.x, (unsigned int) m_tileSize.y);
         crossCorrSource->getTile(tileRect);
         const vector<double>& tileCorrs = crossCorrSource->getCrossCorrelations();
         for (int b=0; b<nBands; ++b)
            correlations[b] += tileCorrs[b];

         ++numTilesSampled;

         if (DEBUG_ENABLED)
         {
            CINFO<<"\nossimEqualityTest:"<<__LINE__<<" results for tile #"<<numTilesSampled<<endl;
            for (int b=0; b<nBands; ++b)
               CINFO<<"\n    correlation["<<b<<"] = "<<correlations[b]<<endl;
         }

      }
   }

   // Take average:
   for (int b=0; b<nBands; ++b)
      correlations[b] = correlations[b] / (double) numTilesSampled;

   if (DEBUG_ENABLED)
   {
      CINFO<<"\nossimEqualityTest:"<<__LINE__<<" results for all tiles: "<<endl;
      for (int b=0; b<nBands; ++b)
         CINFO<<"\n    correlation["<<b<<"] = "<<correlations[b]<<endl;
   }

}

