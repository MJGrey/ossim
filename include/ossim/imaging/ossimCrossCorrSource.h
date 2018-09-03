//**************************************************************************************************
//
//     OSSIM Open Source Geospatial Data Processing Library
//     See top level LICENSE.txt file for license information
//
//**************************************************************************************************
#ifndef ossimCrossCorrSource_HEADER
#define ossimCrossCorrSource_HEADER

#include <ossim/base/ossimIrect.h>
#include <ossim/base/ossimRefPtr.h>
#include <ossim/imaging/ossimImageCombiner.h>
#include <memory>

/*!
 *  Class for computing the normalized cross-correlation between two tiles co-located in 2D space.
 *  This source does not produce an output tile, actually returns a null tile, but does generate
 *  floating point numbers for the cross-correlation between the two tiles. It must be at the end
 *  of a chain (excluding the sequencer). The bands are processed independently. There is one
 *  correlation value per tile per band.
 *
 *  The cross correlation is fully normalized, so if the two tiles are identical, the value
 *  generated in 1.0. Any difference between tiles will produce a value 0.0 < C < 1.0.
 */
class OSSIMDLLEXPORT ossimCrossCorrSource : public ossimImageCombiner
{
public:
   ossimCrossCorrSource();

   explicit ossimCrossCorrSource(ossimConnectableObject::ConnectableObjectList& inputSources);

   ~ossimCrossCorrSource() override = default;

   void initialize() override;

   /** Tile returned is NULL. Must call getCrossCorrelation() after getTile() to get the value. */
   ossimRefPtr<ossimImageData> getTile(const ossimIrect& origin, ossim_uint32 rLevel=0)  override;

   const std::vector<double>& getCrossCorrelations() const { return m_crossCorrelation; }

protected:
   std::vector<double> m_crossCorrelation;

};
#endif /* #ifndef ossimCrossCorrSource_HEADER */
