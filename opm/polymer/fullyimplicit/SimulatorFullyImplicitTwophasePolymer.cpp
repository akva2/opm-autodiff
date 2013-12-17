/*
  Copyright 2013 SINTEF ICT, Applied Mathematics.

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

#include <opm/polymer/fullyimplicit/SimulatorFullyImplicitTwophasePolymer.hpp>
#include <opm/core/utility/parameters/ParameterGroup.hpp>
#include <opm/core/utility/ErrorMacros.hpp>

#include <opm/polymer/fullyimplicit/FullyImplicitTwophasePolymerSolver.hpp>
#include <opm/polymer/fullyimplicit/IncompPropsAdInterface.hpp>
#include <opm/polymer/fullyimplicit/PolymerPropsAd.hpp>
#include <opm/core/grid.h>
#include <opm/core/wells.h>
#include <opm/core/pressure/flow_bc.h>

#include <opm/core/simulator/SimulatorReport.hpp>
#include <opm/core/simulator/SimulatorTimer.hpp>
#include <opm/core/utility/StopWatch.hpp>
#include <opm/core/io/vtk/writeVtkData.hpp>


#include <opm/core/utility/miscUtilities.hpp>

#include <opm/core/grid/ColumnExtract.hpp>
#include <opm/polymer/PolymerState.hpp>
#include <opm/polymer/PolymerInflow.hpp>

#include <boost/filesystem.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/lexical_cast.hpp>

#include <numeric>
#include <fstream>
#include <iostream>
#include <Eigen/Eigen>
namespace Opm
{



    class SimulatorFullyImplicitTwophasePolymer::Impl
    {
    public:
        Impl(const parameter::ParameterGroup& param,
             const UnstructuredGrid& grid,
             const IncompPropsAdInterface& props,
             const PolymerPropsAd&  polymer_props,
             LinearSolverInterface& linsolver,
             const PolymerInflowInterface& polymer_inflow,
             std::vector<double>& src);

        SimulatorReport run(SimulatorTimer& timer,
                            PolymerState& state);

    private:

        // Parameters for output.
        bool output_;
        bool output_vtk_;
        std::string output_dir_;
        int output_interval_;
        // Parameters for well control
        // Observed objects.
        const UnstructuredGrid& grid_;
        const IncompPropsAdInterface& props_;
        const PolymerPropsAd&  polymer_props_;
        const PolymerInflowInterface& polymer_inflow_;
        const std::vector<double>& src_;
        // Solvers
        FullyImplicitTwophasePolymerSolver solver_;
        // Misc. data
        std::vector<int> allcells_;
    };




    SimulatorFullyImplicitTwophasePolymer::SimulatorFullyImplicitTwophasePolymer(const parameter::ParameterGroup& param,
                                                                   const UnstructuredGrid& grid,
                                                                   const IncompPropsAdInterface& props,
                                                                   const PolymerPropsAd& polymer_props,
                                                                   LinearSolverInterface& linsolver,
                                                                   const PolymerInflowInterface&  polymer_inflow,
                                                                   std::vector<double>& src)
    {
        pimpl_.reset(new Impl(param, grid, props, polymer_props, linsolver, polymer_inflow, src));
    }





    SimulatorReport SimulatorFullyImplicitTwophasePolymer::run(SimulatorTimer& timer,
                                                        PolymerState& state)
    {
        return pimpl_->run(timer, state);
    }



    static void outputStateVtk(const UnstructuredGrid& grid,
                               const Opm::PolymerState& state,
                               const int step,
                               const std::string& output_dir)
    {
        // Write data in VTK format.
        std::ostringstream vtkfilename;
        vtkfilename << output_dir << "/vtk_files";
        boost::filesystem::path fpath(vtkfilename.str());
        try {
            create_directories(fpath);
        }
        catch (...) {
            OPM_THROW(std::runtime_error, "Creating directories failed: " << fpath);
        }
        vtkfilename << "/output-" << std::setw(3) << std::setfill('0') << step << ".vtu";
        std::ofstream vtkfile(vtkfilename.str().c_str());
        if (!vtkfile) {
            OPM_THROW(std::runtime_error, "Failed to open " << vtkfilename.str());
        }
        Opm::DataMap dm;
        dm["saturation"] = &state.saturation();
        dm["pressure"] = &state.pressure();
        std::vector<double> cell_velocity;
        Opm::estimateCellVelocity(grid, state.faceflux(), cell_velocity);
        dm["velocity"] = &cell_velocity;
        Opm::writeVtkData(grid, dm, vtkfile);
    }


    static void outputStateMatlab(const UnstructuredGrid& grid,
                                  const Opm::PolymerState& state,
                                  const int step,
                                  const std::string& output_dir)
    {
        Opm::DataMap dm;
        dm["saturation"] = &state.saturation();
        dm["pressure"] = &state.pressure();
        std::vector<double> cell_velocity;
        Opm::estimateCellVelocity(grid, state.faceflux(), cell_velocity);
        dm["velocity"] = &cell_velocity;

        // Write data (not grid) in Matlab format
        for (Opm::DataMap::const_iterator it = dm.begin(); it != dm.end(); ++it) {
            std::ostringstream fname;
            fname << output_dir << "/" << it->first;
            boost::filesystem::path fpath = fname.str();
            try {
                create_directories(fpath);
            }
            catch (...) {
                OPM_THROW(std::runtime_error, "Creating directories failed: " << fpath);
            }
            fname << "/" << std::setw(3) << std::setfill('0') << step << ".txt";
            std::ofstream file(fname.str().c_str());
            if (!file) {
                OPM_THROW(std::runtime_error, "Failed to open " << fname.str());
            }
            file.precision(15);
            const std::vector<double>& d = *(it->second);
            std::copy(d.begin(), d.end(), std::ostream_iterator<double>(file, "\n"));
        }
    }


    
/*
    static void outputWaterCut(const Opm::Watercut& watercut,
                               const std::string& output_dir)
    {
        // Write water cut curve.
        std::string fname = output_dir  + "/watercut.txt";
        std::ofstream os(fname.c_str());
        if (!os) {
            OPM_THROW(std::runtime_error, "Failed to open " << fname);
        }
        watercut.write(os);
    }
    static void outputWellReport(const Opm::WellReport& wellreport,
                                 const std::string& output_dir)
    {
        // Write well report.
        std::string fname = output_dir  + "/wellreport.txt";
        std::ofstream os(fname.c_str());
        if (!os) {
            OPM_THROW(std::runtime_error, "Failed to open " << fname);
        }
        wellreport.write(os);
    }
  */  
    
    
    
    SimulatorFullyImplicitTwophasePolymer::Impl::Impl(const parameter::ParameterGroup& param,
                                               const UnstructuredGrid& grid,
                                               const IncompPropsAdInterface& props,
                                               const PolymerPropsAd&    polymer_props,
                                               LinearSolverInterface& linsolver,
                                               const PolymerInflowInterface& polymer_inflow,
                                               std::vector<double>& src)
        : grid_(grid),
          props_(props),
          polymer_props_(polymer_props),
          polymer_inflow_(polymer_inflow),
          src_ (src),
          solver_(grid_, props_, polymer_props_, linsolver)

    {
        // For output.
        output_ = param.getDefault("output", true);
        if (output_) {
            output_vtk_ = param.getDefault("output_vtk", true);
            output_dir_ = param.getDefault("output_dir", std::string("output"));
            // Ensure that output dir exists
            boost::filesystem::path fpath(output_dir_);
            try {
                create_directories(fpath);
            }
            catch (...) {
                OPM_THROW(std::runtime_error, "Creating directories failed: " << fpath);
            }
            output_interval_ = param.getDefault("output_interval", 1);
        }
        // Misc init.
        const int num_cells = grid.number_of_cells;
        allcells_.resize(num_cells);
        for (int cell = 0; cell < num_cells; ++cell) {
            allcells_[cell] = cell;
        }
    }

    SimulatorReport SimulatorFullyImplicitTwophasePolymer::Impl::run(SimulatorTimer& timer,
                                                              PolymerState& state)
    {

        // Initialisation.
        std::vector<double> porevol;
        Opm::computePorevolume(grid_, props_.porosity(), porevol);

        // const double tot_porevol_init = std::accumulate(porevol.begin(), porevol.end(), 0.0);
        std::vector<double> polymer_inflow_c(grid_.number_of_cells);

        // Main simulation loop.
        Opm::time::StopWatch solver_timer;
        double stime = 0.0;
        Opm::time::StopWatch step_timer;
        Opm::time::StopWatch total_timer;
        total_timer.start();
#if 0
        // These must be changed for three-phase.
        double init_surfvol[2] = { 0.0 };
        double inplace_surfvol[2] = { 0.0 };
        double tot_injected[2] = { 0.0 };
        double tot_produced[2] = { 0.0 };
        Opm::computeSaturatedVol(porevol, state.surfacevol(), init_surfvol);
        Opm::Watercut watercut;
        watercut.push(0.0, 0.0, 0.0);
#endif
        std::vector<double> fractional_flows;
        std::vector<double> well_resflows_phase;
        std::fstream tstep_os;
        if (output_) {
            std::string filename = output_dir_ + "/step_timing.param";
            tstep_os.open(filename.c_str(), std::fstream::out | std::fstream::app);
        }
        while (!timer.done()) {
            // Report timestep and (optionally) write state to disk.
            step_timer.start();
            timer.report(std::cout);
            if (output_ && (timer.currentStepNum() % output_interval_ == 0)) {
                if (output_vtk_) {
                    outputStateVtk(grid_, state, timer.currentStepNum(), output_dir_);
                }
                outputStateMatlab(grid_, state, timer.currentStepNum(), output_dir_);

            }

            SimulatorReport sreport;

                // Run solver.
                // Find inflow rate.
                const double current_time = timer.currentTime();
                double stepsize = timer.currentStepLength();
                polymer_inflow_.getInflowValues(current_time, current_time + stepsize, polymer_inflow_c);
                solver_timer.start();
                solver_.step(timer.currentStepLength(), state, src_, polymer_inflow_c);

                // Stop timer and report.
                solver_timer.stop();
                const double st = solver_timer.secsSinceStart();
                std::cout << "Fully implicit solver took:  " << st << " seconds." << std::endl;

                stime += st;
                sreport.pressure_time = st;


            // Update pore volumes if rock is compressible.

            // The reports below are geared towards two phases only.
#if 0
            // Report mass balances.
            double injected[2] = { 0.0 };
            double produced[2] = { 0.0 };
            Opm::computeInjectedProduced(props_, state, transport_src, stepsize,
                                         injected, produced);
            Opm::computeSaturatedVol(porevol, state.surfacevol(), inplace_surfvol);
            tot_injected[0] += injected[0];
            tot_injected[1] += injected[1];
            tot_produced[0] += produced[0];
            tot_produced[1] += produced[1];
            std::cout.precision(5);
            const int width = 18;
            std::cout << "\nMass balance report.\n";
            std::cout << "    Injected surface volumes:      "
                      << std::setw(width) << injected[0]
                      << std::setw(width) << injected[1] << std::endl;
            std::cout << "    Produced surface volumes:      "
                      << std::setw(width) << produced[0]
                      << std::setw(width) << produced[1] << std::endl;
            std::cout << "    Total inj surface volumes:     "
                      << std::setw(width) << tot_injected[0]
                      << std::setw(width) << tot_injected[1] << std::endl;
            std::cout << "    Total prod surface volumes:    "
                      << std::setw(width) << tot_produced[0]
                      << std::setw(width) << tot_produced[1] << std::endl;
            const double balance[2] = { init_surfvol[0] - inplace_surfvol[0] - tot_produced[0] + tot_injected[0],
                                        init_surfvol[1] - inplace_surfvol[1] - tot_produced[1] + tot_injected[1] };
            std::cout << "    Initial - inplace + inj - prod: "
                      << std::setw(width) << balance[0]
                      << std::setw(width) << balance[1]
                      << std::endl;
            std::cout << "    Relative mass error:            "
                      << std::setw(width) << balance[0]/(init_surfvol[0] + tot_injected[0])
                      << std::setw(width) << balance[1]/(init_surfvol[1] + tot_injected[1])
                      << std::endl;
            std::cout.precision(8);

            // Make well reports.
            watercut.push(timer.currentTime() + timer.currentStepLength(),
                          produced[0]/(produced[0] + produced[1]),
                          tot_produced[0]/tot_porevol_init);
            if (wells_) {
                wellreport.push(props_, *wells_,
                                state.pressure(), state.surfacevol(), state.saturation(),
                                timer.currentTime() + timer.currentStepLength(),
                                well_state.bhp(), well_state.perfRates());
            }
#endif
            sreport.total_time =  step_timer.secsSinceStart();
            if (output_) {
                sreport.reportParam(tstep_os);

                if (output_vtk_) {
                    outputStateVtk(grid_, state, timer.currentStepNum(), output_dir_);
                }
                outputStateMatlab(grid_, state, timer.currentStepNum(), output_dir_);
#if 0
                outputWaterCut(watercut, output_dir_);
                if (wells_) {
                    outputWellReport(wellreport, output_dir_);
                }
#endif
                tstep_os.close();
            }

            // advance to next timestep before reporting at this location
            ++timer;

            // write an output file for later inspection
        }

        total_timer.stop();

        SimulatorReport report;
        report.pressure_time = stime;
        report.transport_time = 0.0;
        report.total_time = total_timer.secsSinceStart();
        return report;
    }





} // namespace Opm
