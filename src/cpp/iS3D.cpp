
#include<iostream>
#include<iomanip>
#include<fstream>
#include<string>
#include <string.h>
#include<sstream>
#include<cmath>
#include<vector>
#include<sys/time.h>
#include "iS3D.h"
#include "Macros.h"
#include "Table.h"
#include "readindata.h"
#include "EmissionFunction.h"
#include "Arsenal.h"
#include "ParameterReader.h"
#include "DeltafData.h"
#ifdef _OPENMP
  #include <omp.h>
#endif


IS3D::IS3D()
{

}


IS3D::~IS3D()
{

}


void IS3D::read_fo_surf_from_memory(std::vector<double> tau_in, std::vector<double> x_in, std::vector<double> y_in, std::vector<double> eta_in,
  std::vector<double> dsigma_tau_in, std::vector<double> dsigma_x_in, std::vector<double> dsigma_y_in, std::vector<double> dsigma_eta_in,
  std::vector<double> E_in, std::vector<double> T_in, std::vector<double> P_in, std::vector<double> ux_in, std::vector<double> uy_in, std::vector<double> un_in,
  std::vector<double> pixx_in, std::vector<double> pixy_in, std::vector<double> pixn_in, std::vector<double> piyy_in, std::vector<double> piyn_in, std::vector<double> pinn_in,
  std::vector<double> Pi_in
  )
{
  tau = tau_in;
  x = x_in;
  y = y_in;
  eta = eta_in;
  dsigma_tau = dsigma_tau_in;
  dsigma_x = dsigma_x_in;
  dsigma_y = dsigma_y_in;
  dsigma_eta = dsigma_eta_in;
  E = E_in;
  T = T_in;
  P = P_in;
  ux = ux_in;
  uy = uy_in;
  un = un_in;
  pixx = pixx_in;
  pixy = pixy_in;
  pixn = pixn_in;
  piyy = piyy_in;
  piyn = piyn_in;
  pinn = pinn_in;       // pinn is extraneous
  Pi = Pi_in;
}


void IS3D::run_particlization(int fo_from_file)
{
  printf("\n::::::::::::::::::::::::::::::::::::::::\n");
  printf("::                                    ::\n");
  printf("::    Starting iS3D particlization    ::\n");
  printf("::                                    ::\n");
  printf("::::::::::::::::::::::::::::::::::::::::\n\n");

#ifdef PRINT_PROCESSOR
  print_processor();                                    // print processor in use (for benchmarking)
#endif
#ifdef _OPENMP
    printf("OpenMP acceleration is on...\n\n");
#else
    printf("OpenMP acceleration is off...\n\n");
#endif

  printf("===================================================\n");
  printf("Reading in parameters...\n\n");
  ParameterReader *paraRdr = new ParameterReader;       // parameter reader class
  paraRdr->readFromFile("iS3D_parameters.dat");
  int include_baryon = paraRdr->getVal("include_baryon");
  int operation = paraRdr->getVal("operation");
  int test_sampler = paraRdr->getVal("test_sampler");

#ifdef PRINT_PARAMETERS
  paraRdr->echo();
#endif


  printf("===================================================\n");
  printf("Reading in freezeout surface...\n\n");
  long FO_length = 0;
  FO_data_reader freeze_out_data(paraRdr, "input");     // freezeout reader class

  if(fo_from_file == 1)
  {
    FO_length = freeze_out_data.get_number_cells();     // get length of freezeout surface from file
  }
  else
  {
    FO_length = tau.size();                             // get length of freezeout surface from memory
  }


  FO_surf *surf_ptr = new FO_surf[FO_length];           // freezeout surface pointer

  if(fo_from_file == 1)
  {
    freeze_out_data.read_freezeout_surface(surf_ptr);   // load freezeout surface info from file
  }
  else
  {
    printf("from memory (please check that you've already undone hbarc = 1 units, tau factors from hydro module)...\n\n");

    double T_avg = 0;                                   // average thermodynamic variables across freezeout surface
    double E_avg = 0;
    double P_avg = 0;
    double muB_avg = 0;
    double nB_avg = 0;
    double max_volume = 0;

    for(long icell = 0; icell < FO_length; icell++)     // load freezeout surface info from memory
    {
      // Please check that the units match!

      // contravariant spacetime position x^\mu
      surf_ptr[icell].tau = tau[icell];                 // \tau [fm]
      surf_ptr[icell].x = x[icell];                     // x [fm]
      surf_ptr[icell].y = y[icell];                     // y [fm]
      surf_ptr[icell].eta = eta[icell];                 // \eta_s [1]

      // covariant surface normal vector
      surf_ptr[icell].dat = dsigma_tau[icell];          // d\sigma_\tau [fm^-2]
      surf_ptr[icell].dax = dsigma_x[icell];            // d\sigma_x [fm^-2]
      surf_ptr[icell].day = dsigma_y[icell];            // d\sigma_y [fm^-2]
      surf_ptr[icell].dan = dsigma_eta[icell];          // d\sigma_\eta [fm^-3]

      // contravariant fluid velocity u^\mu
      surf_ptr[icell].ux = ux[icell];                   // u^x [1]
      surf_ptr[icell].uy = uy[icell];                   // u^y [1]
      surf_ptr[icell].un = un[icell];                   // u^\eta [fm^-1]

      // thermodynamic variables
      surf_ptr[icell].E = E[icell];                     // energy density [GeV/fm^3]
      surf_ptr[icell].T = T[icell];                     // temperature [GeV]
      surf_ptr[icell].P = P[icell];                     // equilibrium pressure [GeV/fm^3]

      // contravariant shear stress pi^\mu\nu
      surf_ptr[icell].pixx = pixx[icell];               // pi^xx [GeV/fm^3]
      surf_ptr[icell].pixy = pixy[icell];               // pi^xy [GeV/fm^3]
      surf_ptr[icell].pixn = pixn[icell];               // pi^x\eta [GeV/fm^4]
      surf_ptr[icell].piyy = piyy[icell];               // pi^yy [GeV/fm^3]
      surf_ptr[icell].piyn = piyn[icell];               // pi^y\eta [GeV/fm^4]

      // bulk viscous pressure
      surf_ptr[icell].bulkPi = Pi[icell];               // Pi [GeV/fm^3]


      // JETSCAPE doesn't consider (muB, nB, V^\mu) atm


      // compute averaged thermodynamic quantities (for fast_mode = 1)
      double E_s = E[icell];
      double T_s = T[icell];
      double P_s = P[icell];
      double muB_s = 0;                                 // note: will need updating...
      double nB_s = 0;

      double tau_s = tau[icell];
      double tau2 = tau_s * tau_s;

      double ux_s = ux[icell];
      double uy_s = uy[icell];
      double un_s = un[icell];
      double ut = sqrt(1.  +  ux_s * ux_s  +  uy_s * uy_s  +  tau2 * un_s * un_s);

      double dat = dsigma_tau[icell];
      double dax = dsigma_x[icell];
      double day = dsigma_y[icell];
      double dan = dsigma_eta[icell];

      double uds = ut * dat  +  ux_s * dax  +  uy_s * day  +  un_s * dan;           // u^\mu . d\sigma_\mu
      double ds_ds = dat * dat  -  dax * dax  -  day * day  -  dan * dan / tau2;    // d\sigma^\mu . d\sigma_\mu
      double ds_max = fabs(uds)  +  sqrt(fabs(uds * uds  -  ds_ds));                // max volume element |ds|

      E_avg += (E_s * ds_max);      // append values
      T_avg += (T_s * ds_max);
      P_avg += (P_s * ds_max);
      muB_avg += (muB_s * ds_max);
      nB_avg += (nB_s * ds_max);
      max_volume += ds_max;

    }

    T_avg /= max_volume;            // divide by total max volume
    E_avg /= max_volume;
    P_avg /= max_volume;
    muB_avg /= max_volume;
    nB_avg /= max_volume;

    // write averaged thermodynamic variables to file
    ofstream thermal_average("tables/thermodynamic/average_thermodynamic_quantities.dat", ios_base::out);
    thermal_average << setprecision(15) << T_avg << "\n" << E_avg << "\n" << P_avg << "\n" << muB_avg << "\n" << nB_avg;
    thermal_average.close();
  }

  printf("Number of freezeout cells = %ld\n", FO_length);


  printf("===================================================\n");
  printf("Reading in particle info from ");
  particle_info *particle_data = new particle_info[Maxparticle];  // particle info pointer
  PDG_Data pdg(paraRdr);                                          // PDG class
  int Nparticle = pdg.read_resonances(particle_data);             // get number of resonances in PDG file

  printf("===================================================\n");
  printf("Reading in chosen particles table from PDG/chosen_particles.dat... (please check if 1 blank line eof)\n\n");

  string chosen_particles_list;

  if(test_sampler == 1){
    chosen_particles_list = "PDG/chosen_particles_pikp.dat";      // if testing sampler and getting continuous distributions, just choose pi/k/p/pbar
  } else {
    chosen_particles_list = "PDG/chosen_particles.dat";
  }

  Table chosen_particles(chosen_particles_list);             // chosen particles table

  printf("Number of chosen particles = %ld\n", chosen_particles.getNumberOfRows());

  printf("===================================================\n");
  Deltaf_Data *df_data = new Deltaf_Data(paraRdr);               // df data pointer
  df_data->load_df_coefficient_data();                            // read in df coefficient tables

  if(!include_baryon)
  {
    df_data->construct_cubic_splines();                           // prepare cubic spline interpolation (muB = 0)
    df_data->compute_jonah_coefficients(particle_data, Nparticle);// compute PTB exclusive coefficients (muB = 0)
  }

  df_data->compute_particle_densities(particle_data, Nparticle);  // compute resonances' particle density (T = T_avg, muB = muB_avg)
  df_data->test_df_coefficients(-0.1);                            // test df coefficients for bulk pressure Pi = -Peq/10


  printf("===================================================\n");
  printf("Reading in momentum and spacetime rapidity tables from tables/...\n\n");

  string pT_tab_path, phi_tab_path, y_tab_path, eta_tab_path;

  if(operation == 1 && test_sampler == 1){
    pT_tab_path = "tables/all_tables/pT/pT_gauss_table_24pt.dat";                   // pT table
    phi_tab_path = "tables/all_tables/phi/phi_gauss_table_24pt.dat";                 // phi table
    y_tab_path = "tables/all_tables/y_table_half.dat";                     // y table (for 3+1d smooth CFF)
    eta_tab_path = "tables/all_tables/eta_table_half.dat";       // eta table (for 2+1d smooth CFF)
  } else {
    pT_tab_path = "tables/momentum/pT_table.dat";                   // pT table
    phi_tab_path = "tables/momentum/phi_table.dat";                 // phi table
    y_tab_path = "tables/momentum/y_table.dat";                     // y table (for 3+1d smooth CFF)
    eta_tab_path = "tables/spacetime_rapidity/eta_table.dat";       // eta table (for 2+1d smooth CFF)
  }
  
  Table pT_tab(pT_tab_path);                   // pT table
  Table phi_tab(phi_tab_path);                 // phi table
  Table y_tab(y_tab_path);                     // y table (for 3+1d smooth CFF)
  Table eta_tab(eta_tab_path);                 // eta table (for 2+1d smooth CFF)

  printf("===================================================\n");
  // emission function class (continuous or sampled particle spectra)
  EmissionFunctionArray efa(paraRdr, &chosen_particles, &pT_tab, &phi_tab, &y_tab, &eta_tab, particle_data, Nparticle, surf_ptr, FO_length, df_data);

  std::vector<std::vector<Sampled_Particle>> particle_event_list_in;    // sampled particle lists (JETSCAPE)
  efa.calculate_spectra(particle_event_list_in);                        // compute particle spectra from Cooper-Frye formula


  // int operation = paraRdr->getVal("operation");

  // if(operation == 2)
  // {
  //   printf("\nCopying final particle list to memory (JETSCAPE)\n");
  //   printf("Event particle list contains %ld events\n", particle_event_list_in.size());

  //   final_particles_ = particle_event_list_in;                    // store particlization events in memory to pass to afterburner module in JETSCAPE
  // }

  delete paraRdr;                                                 // delete pointers
  delete [] surf_ptr;
  delete [] particle_data;
  delete df_data;
  printf("Done. Goodbye!\n");
}




