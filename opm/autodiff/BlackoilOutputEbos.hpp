/*
  Copyright (c) 2017 IRIS AS

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef OPM_BLACKOILOUTPUTEBOS_HEADER_INCLUDED
#define OPM_BLACKOILOUTPUTEBOS_HEADER_INCLUDED


#include <ebos/eclproblem.hh>
#include <ewoms/common/start.hh>

#include <opm/grid/UnstructuredGrid.h>
#include <opm/simulators/timestepping/SimulatorTimerInterface.hpp>
#include <opm/core/utility/DataMap.hpp>
#include <opm/common/ErrorMacros.hpp>
#include <opm/common/OpmLog/OpmLog.hpp>
#include <opm/core/utility/miscUtilities.hpp>
#include <opm/common/utility/parameters/ParameterGroup.hpp>
#include <opm/core/wells/DynamicListEconLimited.hpp>
#include <opm/core/simulator/SimulatorReport.hpp>
#include <opm/core/wells/WellsManager.hpp>
#include <opm/output/data/Cells.hpp>
#include <opm/output/data/Solution.hpp>

#include <opm/autodiff/Compat.hpp>

#include <opm/autodiff/WellStateFullyImplicitBlackoil.hpp>

#include <opm/parser/eclipse/Units/UnitSystem.hpp>
#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>
#include <opm/parser/eclipse/EclipseState/InitConfig/InitConfig.hpp>

#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <map>

#include <boost/filesystem.hpp>

#ifdef HAVE_OPM_GRID
#include <opm/grid/CpGrid.hpp>
#endif
namespace Opm
{


    /// Extra data to read/write for OPM restarting
    struct ExtraData
    {
        double suggested_step = -1.0;
    };


    /** \brief Wrapper ECL output. */
    template<class TypeTag>
    class BlackoilOutputEbos
    {
    public:

        typedef typename GET_PROP_TYPE(TypeTag, Simulator) Simulator;
        typedef typename GET_PROP_TYPE(TypeTag, Grid) Grid;
        typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;
        typedef typename GET_PROP_TYPE(TypeTag, ElementContext) ElementContext;
        // constructor creating different sub writers
        BlackoilOutputEbos(Simulator& ebosSimulator,
                           const ParameterGroup& param)
            : output_( [ &param ] () -> bool {
                    // If output parameter is true or all, then we do output
                    const std::string outputString = param.getDefault("output", std::string("all"));
                    return ( outputString == "all" ||  outputString == "true" );
                }()
                ),
            ebosSimulator_(ebosSimulator),
            phaseUsage_(phaseUsageFromDeck(eclState()))
        {}

        /*!
         * \brief Write a blackoil reservoir state to disk for later inspection with
         *        visualization tools like ResInsight. This function will extract the
         *        requested output cell properties specified by the RPTRST keyword
         *        and write these to file.
         */
        template<class SimulationDataContainer, class Model>
        void writeTimeStep(const SimulatorTimerInterface& timer,
                           const SimulationDataContainer& /*reservoirStateDummy*/,
                           const Opm::WellStateFullyImplicitBlackoil& /*wellStateDummy*/,
                           const Model& physicalModel,
                           const bool substep = false,
                           const double nextstep = -1.0,
                           const SimulatorReport& simulatorReport = SimulatorReport())
        {
            if( output_ )
            {
                // Add TCPU if simulatorReport is not defaulted.
                const double totalSolverTime = simulatorReport.solver_time;

                const Opm::WellStateFullyImplicitBlackoil& localWellState = physicalModel.wellModel().wellState();

                // The writeOutput expects a local data::solution vector and a local data::well vector.
                auto localWellData = localWellState.report(phaseUsage_, Opm::UgGridHelpers::globalCell(grid()) );
                ebosSimulator_.problem().writeOutput(localWellData, timer.simulationTimeElapsed(), substep, totalSolverTime, nextstep);
            }
        }

        template <class SimulationDataContainer, class WellState>
        void initFromRestartFile(const PhaseUsage& /*phaseUsage*/,
                                 const Grid& /*grid */,
                                 SimulationDataContainer& simulatorstate,
                                 WellState& wellstate,
                                 ExtraData& extra)   {

            std::vector<RestartKey> extra_keys = {
              {"OPMEXTRA" , Opm::UnitSystem::measure::identity, false}
            };

            // gives a dummy dynamic_list_econ_limited
            DynamicListEconLimited dummy_list_econ_limited;
            const auto& defunct_well_names = ebosSimulator_.vanguard().defunctWellNames();
            WellsManager wellsmanager(eclState(),
                                      schedule(),
                                      // The restart step value is used to identify wells present at the given
                                      // time step. Wells that are added at the same time step as RESTART is initiated
                                      // will not be present in a restart file. Use the previous time step to retrieve
                                      // wells that have information written to the restart file.
                                      std::max(eclState().getInitConfig().getRestartStep() - 1, 0),
                                      Opm::UgGridHelpers::numCells(grid()),
                                      Opm::UgGridHelpers::globalCell(grid()),
                                      Opm::UgGridHelpers::cartDims(grid()),
                                      Opm::UgGridHelpers::dimensions(grid()),
                                      Opm::UgGridHelpers::cell2Faces(grid()),
                                      Opm::UgGridHelpers::beginFaceCentroids(grid()),
                                      dummy_list_econ_limited,
                                      grid().comm().size() > 1,
                                      defunct_well_names);

            const Wells* wells = wellsmanager.c_wells();

            std::vector<RestartKey> solution_keys = {};
            auto restart_values = ebosSimulator_.problem().eclIO().loadRestart(solution_keys, extra_keys);

            const int nw = wells->number_of_wells;
            if (nw > 0) {
                wellstate.resize(wells, simulatorstate, phaseUsage_ ); //Resize for restart step
                wellsToState( restart_values.wells, phaseUsage_, wellstate );
            }

            if (restart_values.hasExtra("OPMEXTRA")) {
                std::vector<double> opmextra = restart_values.getExtra("OPMEXTRA");
                assert(opmextra.size() == 1);
                extra.suggested_step = opmextra[0];
            } else {
                OpmLog::warning("Restart data is missing OPMEXTRA field, restart run may deviate from original run.");
                extra.suggested_step = -1.0;
            }
        }

        const Grid& grid()
        { return ebosSimulator_.vanguard().grid(); }

        const Schedule& schedule() const
        { return ebosSimulator_.vanguard().schedule(); }

        const EclipseState& eclState() const
        { return ebosSimulator_.vanguard().eclState(); }

        bool isRestart() const {
            const auto& initconfig = eclState().getInitConfig();
            return initconfig.restartRequested();
        }   

    protected:
        const bool output_;
        Simulator& ebosSimulator_;
        Opm::PhaseUsage phaseUsage_;
    };





}
#endif
