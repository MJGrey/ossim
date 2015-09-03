//*******************************************************************
// Copyright (C) 2005 Garrett Potts
//
// License:  LGPL
//
// See LICENSE.txt file in the top level directory for more details.
//
// Author: Garrett Potts
//
//
//*******************************************************************
//  $Id: ossimStreamFactory.h 22653 2014-02-28 14:45:38Z gpotts $
//
#ifndef ossimStreamFactory_HEADER
#define ossimStreamFactory_HEADER
#include <ossim/base/ossimStreamFactoryBase.h>
#include <ossim/base/ossimIoStream.h>

class OSSIM_DLL ossimStreamFactory : public ossimStreamFactoryBase
{
public:
   static ossimStreamFactory* instance();
   virtual ~ossimStreamFactory();
 
   virtual ossimRefPtr<ossimIFStream>
      createNewIFStream(const ossimFilename& file,
                        std::ios_base::openmode openMode) const;

   
protected:
   ossimStreamFactory();
   ossimStreamFactory(const ossimStreamFactory&);
   static ossimStreamFactory* theInstance;
   
};

#endif
