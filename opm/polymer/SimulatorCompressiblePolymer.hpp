/*
  Copyright 2012 SINTEF ICT, Applied Mathematics.

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

#ifndef OPM_SIMULATORCOMPRESSIBLEPOLYMER_HEADER_INCLUDED
#define OPM_SIMULATORCOMPRESSIBLEPOLYMER_HEADER_INCLUDED

#include <opm/core/simulator/SimulatorReport.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>

struct UnstructuredGrid;
struct Wells;
struct FlowBoundaryConditions;

namespace Opm
{
    class ParameterGroup;
    class BlackoilPropertiesInterface;
    class PolymerProperties;
    class RockCompressibility;
    class WellsManager;
    class PolymerInflowInterface;
    class LinearSolverInterface;
    class SimulatorTimer;
    class PolymerBlackoilState;
    class WellState;
    struct SimulatorReport;

    /// Class collecting all necessary components for a two-phase simulation.
    class SimulatorCompressiblePolymer
    {
    public:
        /// Initialise from parameters and objects to observe.
        /// \param[in] param       parameters, this class accepts the following:
        ///     parameter (default)            effect
        ///     -----------------------------------------------------------
        ///     output (true)                  write output to files?
        ///     output_dir ("output")          output directoty
        ///     output_interval (1)            output every nth step
        ///     nl_pressure_residual_tolerance (0.0) pressure solver residual tolerance (in Pascal)
        ///     nl_pressure_change_tolerance (1.0)   pressure solver change tolerance (in Pascal)
        ///     nl_pressure_maxiter (10)       max nonlinear iterations in pressure
        ///     nl_maxiter (30)                max nonlinear iterations in transport
        ///     nl_tolerance (1e-9)            transport solver absolute residual tolerance
        ///     num_transport_substeps (1)     number of transport steps per pressure step
        ///     use_segregation_split (false)  solve for gravity segregation (if false,
        ///                                    segregation is ignored).
        ///
        /// \param[in] grid             grid data structure
        /// \param[in] props            fluid and rock properties
        /// \param[in] poly_props       polymer properties
        /// \param[in] rock_comp_props  if non-null, rock compressibility properties
        /// \param[in] wells_manager    well manager, may manage no (null) wells
        /// \param[in] polymer_inflow   polymer inflow controls
        /// \param[in] linsolver        linear solver
        /// \param[in] gravity          if non-null, gravity vector
        SimulatorCompressiblePolymer(const ParameterGroup& param,
                                     const UnstructuredGrid& grid,
                                     const BlackoilPropertiesInterface& props,
                                     const PolymerProperties& poly_props,
                                     const RockCompressibility* rock_comp_props,
                                     WellsManager& wells_manager,
                                     const PolymerInflowInterface& polymer_inflow,
                                     LinearSolverInterface& linsolver,
                                     const double* gravity);

        /// Run the simulation.
        /// This will run succesive timesteps until timer.done() is true. It will
        /// modify the reservoir and well states.
        /// \param[in,out] timer       governs the requested reporting timesteps
        /// \param[in,out] state       state of reservoir: pressure, fluxes, polymer concentration,
        ///                            saturations, surface volumes.
        /// \param[in,out] well_state  state of wells: bhp, perforation rates
        /// \return                    simulation report, with timing data
        SimulatorReport run(SimulatorTimer& timer,
                            PolymerBlackoilState& state,
                            WellState& well_state);

        /// return the statistics if the nonlinearIteration() method failed.
        ///
        /// NOTE: for the flow_legacy simulator family this method is a stub, i.e. the
        /// failure report object will *not* contain any meaningful data.
        const SimulatorReport& failureReport() const
        { return failureReport_; }

    private:
        class Impl;

        SimulatorReport failureReport_;

        // Using shared_ptr instead of scoped_ptr since scoped_ptr requires complete type for Impl.
        boost::shared_ptr<Impl> pimpl_;
    };

} // namespace Opm

#endif // OPM_SIMULATORCOMPRESSIBLEPOLYMER_HEADER_INCLUDED
