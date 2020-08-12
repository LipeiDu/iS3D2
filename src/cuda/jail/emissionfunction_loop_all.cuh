#ifndef EMISSIONFUNCTION_H
#define EMISSIONFUNCTION_H

#include<string>
#include<vector>
#include "Table.cuh"
#include "main.cuh"
#include "ParameterReader.cuh"
#include "deltafReader.cuh"
using namespace std;

class EmissionFunctionArray
{
private:

  // control parameters
  int MODE;      
  int DF_MODE;   
  int DIMENSION; 
  int INCLUDE_BARYON;
  int INCLUDE_SHEAR_DELTAF;
  int INCLUDE_BULK_DELTAF;
  int INCLUDE_BARYONDIFF_DELTAF;
  int OUTFLOW;
  int REGULATE_DELTAF;
  

  // freezeout surface
  FO_surf *surf_ptr;
  long FO_length;


  // momentum tables
  Table *pT_tab;
  Table *phi_tab;
  Table *y_tab;
  Table *eta_tab;

  long pT_tab_length;
  long phi_tab_length;
  long y_tab_length;
  long eta_tab_length;


  // particle info
  particle_info* particles;  
  long Nparticles;              // number of pdg particles
  long npart;                   // number of chosen particles
  long *chosen_particles_table; // stores the pdg index of the chosen particle (to access chosen particle properties)
  
  
  // df coefficients
  Deltaf_Data *df_data;


  // particle spectra of chosen particle species
  long momentum_length;
  long spectra_length;
  double *dN_pTdpTdphidy_momentum;    
  double *dN_pTdpTdphidy_spectra;    


public:
  EmissionFunctionArray(ParameterReader* paraRdr_in, Table* chosen_particle, Table* pT_tab_in, Table* phi_tab_in, Table* y_tab_in, Table* eta_tab_in,
                        particle_info* particles_in, int Nparticles, FO_surf* FOsurf_ptr_in, long FO_length_in, Deltaf_Data *df_data_in);
  ~EmissionFunctionArray();

  void write_dN_2pipTdpTdy_toFile(long *MCID);

  void calculate_spectra();

};

#endif
