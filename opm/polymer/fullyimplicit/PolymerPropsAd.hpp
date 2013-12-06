#ifndef OPM_POLYMERPROPSAD_HEADED_INLCUDED
#define OPM_POLYMERPROPSAD_HEADED_INLCUDED


#include <cmath>
#include <vector>
#include <opm/polymer/fullyimplicit/AutoDiffBlock.hpp>
#include <opm/polymer/fullyimplicit/AutoDiffHelpers.hpp>
#include <opm/polymer/polymerProperties.hpp>
namespace Opm {

    
    class PolymerPropsAd 
    {
    public:
/*        PolymerPropsAd(const int num_cells;
                       const double mix_param,
                       const std::vector<double>& c_max,
                       const std::vector<double>& c_vals_visc,
                       const std::vector<double>& visc_mult_vals);
        

        double num_cells() const;
        double cMax() const;
        double mixParam() const;

        typedef AutoDiffBlock<double> ADB;
        typedef ADB::V V;


        V muM(const V& c,
              const double* visc) const;
        ADB muM(const ADB& c
                const double* visc) const;

        V ToddLongstaff(const double mix_param,
                        const V& muM,
                        const V& mu) const;

        ADB ToddLongstaff(const double mix_param,
                          const ADB& muM,
                          const ADB& mu) const;
       
        V muPolyEff(const double mix_param,
                    const V& muM,
                    const V& muPoly) const;
    
        ADB muPolyEff(const double mix_param,
                      const ADB& muM,
                      const ADB& muPoly) const;

        V muWatEff(const double mix_param,
                   const std::vector<double>& c_max,
                   const V& c,
                   const V& muM,
                   const V& muWat,
                   const V& muPolyEff) const;

        ADB muWatEff(const double mix_param,
                     const std::vector<double>& c_max,
                     const ADB& c,
                     const ADB& muM,
                     const ADB& muWat,
                     const ADB& muPolyEff) const;
*/
        typedef AutoDiffBlock<double> ADB;
        typedef ADB::V V;
        PolymerPropsAd(const PolymerPropperties& polyprops);
        ~PolymerPropsAd();
    private:
        const PolymerProperties polyprops_;
    };

    
}













#endif// OPM_POLYMERPROPSAD_HEADED_INLCUDED
