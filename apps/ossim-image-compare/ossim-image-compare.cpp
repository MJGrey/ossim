//----------------------------------------------------------------------------
//
// License:  See top level LICENSE.txt file.
//
// File: ossim-image-compare.cpp
//
// Author:  Oscar Kramer
//
// Description: Compares pixel data between two images. Returns with 0 if same or 1 of different.
//              The input formats can be different -- the pixels are compared after any 
//              unpacking and decompression. Only R0 is compared.
//
// $Id: ossim-image-compare.cpp 19753 2011-06-13 15:20:31Z dburken $
//----------------------------------------------------------------------------

#include <ossim/base/ossimArgumentParser.h>
#include <ossim/base/ossimException.h>
#include <ossim/base/ossimNotify.h>
#include <ossim/init/ossimInit.h>
#include <iostream>
#include <ossim/imaging/ossimImageHandlerRegistry.h>
#include <ossim/imaging/ossimImageSourceSequencer.h>
#include <ossim/util/ossimEqualityTest.h>

using namespace std;

int main(int argc, char *argv[])
{
   ossimArgumentParser ap(&argc, argv);
   ossimInit::instance()->addOptions(ap);
   ossimInit::instance()->initialize(ap);

   try
   {
      ossimFilename f1 (argv[1]);
      ossimFilename f2 (argv[2]);
      cout << "\nComparing <"<<f1<<"> to <"<<f2<<">..."<<endl;

      ossimRefPtr< ossimEqualityTest > testTool = new ossimEqualityTest;
      if (!testTool->initialize(ap))
         return 1;

      if (!testTool->execute())
         return 1;

      if (testTool->testPassed())
      {
         cout << "No effective differences found.\n" << endl;
         return 0;
      }
      else
      {
         cout << "Differences found!\n" << endl;
         return 1;
      }
   }
   catch (const ossimException& e)
   {
      ossimNotify(ossimNotifyLevel_WARN) << e.what() << std::endl;
      return 1;
   }
   
}
