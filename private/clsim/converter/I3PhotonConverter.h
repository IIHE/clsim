/**
 * copyright  (C) 2011
 *
 * $Id$
 *
 * @version $Revision$
 * @date $LastChangedDate$
 * @author Claudio Kopper <claudio.kopper@nikhef.nl>
 */

#include "tableio/I3ConverterFactory.h"
#include "tableio/converter/I3MapConverter.h"

#include "clsim/I3Photon.h"

class I3PhotonConverter : public I3ConverterImplementation<I3Photon>
{
public:
    typedef booked_type value_type;

    void AddFields(I3TableRowDescriptionPtr, const value_type&  = value_type());
    void FillSingleRow(const value_type&, I3TableRowPtr);

private:
    I3TableRowDescriptionPtr CreateDescription(const I3Photon &photon); 
    std::size_t FillRows(const I3Photon &photon, I3TableRowPtr rows);
};
    
typedef I3MapOMKeyVectorConverter<I3PhotonConverter, I3PhotonSeriesMap> I3PhotonSeriesMapConverter;
