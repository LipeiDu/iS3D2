#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <vector>
#include <stdio.h>
#include <random>
#include <chrono>
#include <limits>
#include <array>

#include "iS3D.h"
#include "AnisoVariables.h"
#include "EmissionFunction.h"
#include "Momentum.h"
#include "Arsenal.h"
#include "Macros.h"
#include "GaussThermal.h"

using namespace std;


inline double canonical(default_random_engine & generator)
{
  // random number between [0,1)
  return generate_canonical<double, numeric_limits<double>::digits>(generator);
}


inline double uniform_rapidity_distribution(default_random_engine & generator, double y_max)
{
  // uniformly sample the rapidity E [-y_max, y_max); open bound doesn't matter
  double random_number = generate_canonical<double, numeric_limits<double>::digits>(generator);

  return y_max * (2.0 * random_number - 1.0);
}


double pion_thermal_weight_max(double x, double chem)
{
  // rescale the pion thermal weight w_eq by the max value if m/T < 0.8554 (the max is local)
  // we assume no pion chemical potential (i.e. ignore non-equilibrium chemical potential EoS models)

  if(chem != 0.0)
  {
    printf("pion_thermal_weight_max flag: pion has chemical potential\n");
  }

  // x = mass / T < 0.8554
  double x2 = x * x;
  double x3 = x2 * x;
  double x4 = x3 * x;

  // rational polynomial fit of the max value
  double max = (143206.88623164667 - 95956.76008684626*x - 21341.937407169076*x2 + 14388.446116867359*x3 -
     6083.775788504437*x4)/
    (-0.3541350577684533 + 143218.69233952634*x - 24516.803600065778*x2 - 115811.59391199696*x3 +
     35814.36403387459*x4);

  if(x < 0.1)
  {
    printf("pion_thermal_weight_max flag: interpolation outside data range, extrapolating fit...\n");
  }

  double buffer = 1.00001; // ensures rescaled w_eq <= 1.0 numerically

  return buffer * max;
}




double estimate_mean_particle_number(double equilibrium_density, double bulk_density, double diffusion_density, double ds_time, double ds_space, double bulkPi, double Vdsigma, double z, double delta_z, bool feqmod_breaks_down, int df_mode)
{
  //double particle_number = 0.0;

  switch(df_mode)
  {
    case 1: // 14 moment
    case 2: // Chapman Enskog
    case 3: // PTM feqmod
    {
      chapman_enskog:

      return ds_time * (equilibrium_density  +  bulkPi * bulk_density)  -  ds_space * Vdsigma * diffusion_density;

      break;
    }
    case 4: // PTB feqmod
    {
      // Jonah's distribution doesn't work for nonzero muB (violates charge conservation)
      if(!feqmod_breaks_down)
      {
        return ds_time * z * equilibrium_density;
      }
      else
      {
        return ds_time * (1.0 + delta_z) * equilibrium_density;
      }

      break;
    }
    case 5: // PTM famod
    {
      goto chapman_enskog;    // for estimating total particle yield in sampler

      break;
    }
    default:
    {
      printf("\nEstimate particle yield error: please set df_mode = (1,2,3,4,5)\n");
      exit(-1);
    }
  } // df_mode

  //return particle_number;
}


double fast_max_particle_number(double equilibrium_density, double bulk_density, double bulkPi, double z, bool feqmod_breaks_down, int df_mode)
{
  //double particle_density = 0.0;

  switch(df_mode)
  {
    case 1: // 14 moment
    case 2: // Chapman Enskog
    {
      linear_df:

      return 2.0 * equilibrium_density;
      break;
    }
    case 3: // Mike
    {
      if(feqmod_breaks_down) goto linear_df;

      return equilibrium_density  +  bulkPi * bulk_density;
      break;
    }
    case 4: // Jonah
    {
      if(feqmod_breaks_down) goto linear_df;

      return z * equilibrium_density;

      break;
    }
    default:
    {
      printf("\nFast max particle number error: please set df_mode = (1,2,3,4)\n");
      exit(-1);
    }
  } // df_mode

  //if(particle_density < 0.0) printf("Fast max particle error: particle number is negative\n");

  //eturn particle_density;
}


double max_particle_number(double mbar, double degeneracy, double sign, double baryon, double T, double alphaB, double bulkPi, deltaf_coefficients df, bool feqmod_breaks_down, Gauss_Laguerre * laguerre, int df_mode, int include_baryon, double neq_fact, double J20_fact)
{
  double particle_density = 0.0;

  switch(df_mode)
  {
    case 1: // 14 moment
    case 2: // Chapman Enskog
    {
      linear_df:

      const int laguerre_pts = laguerre->points;
      double * pbar_root1 = laguerre->root[1];
      double * pbar_weight1 = laguerre->weight[1];

      double equilibrium_density = neq_fact * degeneracy * GaussThermal(neq_int, pbar_root1, pbar_weight1, laguerre_pts, mbar, alphaB, baryon, sign);

      particle_density = 2.0 * equilibrium_density;

      break;
    }
    case 3: // modified (Mike)
    {
      if(feqmod_breaks_down) goto linear_df;

      double G = df.G;
      double F = df.F;
      double betabulk = df.betabulk;

      // compute the linearized density
      const int laguerre_pts = laguerre->points;
      double * pbar_root1 = laguerre->root[1];
      double * pbar_root2 = laguerre->root[2];
      double * pbar_weight1 = laguerre->weight[1];
      double * pbar_weight2 = laguerre->weight[2];

      double equilibrium_density = neq_fact * degeneracy * GaussThermal(neq_int, pbar_root1, pbar_weight1, laguerre_pts, mbar, alphaB, baryon, sign);
      double J10 = 0.0;
      if(include_baryon)
      {
        J10 = neq_fact * degeneracy * GaussThermal(J10_int, pbar_root1, pbar_weight1, laguerre_pts, mbar, alphaB, baryon, sign);
      }
      double J20 = J20_fact * degeneracy * GaussThermal(J20_int, pbar_root2, pbar_weight2, laguerre_pts, mbar, alphaB, baryon, sign);
      double bulk_density = (equilibrium_density + (baryon * J10 * G) + (J20 * F / T / T)) / betabulk;

      particle_density = equilibrium_density  +  bulkPi * bulk_density;

      break;
    }
    case 4: // modified (Jonah)
    {
      if(feqmod_breaks_down) goto linear_df;

      double z = df.z;

      const int laguerre_pts = laguerre->points;
      double * pbar_root1 = laguerre->root[1];
      double * pbar_weight1 = laguerre->weight[1];

      double equilibrium_density = neq_fact * degeneracy * GaussThermal(neq_int, pbar_root1, pbar_weight1, laguerre_pts, mbar, 0.0, 0.0, sign);

      particle_density = z * equilibrium_density;

      break;
    }
    default:
    {
      printf("\nMax particle number error: please set df_mode = (1,2,3,4)\n");
      exit(-1);
    }
  } // df_mode

  if(particle_density < 0.0) printf("Error: particle number is negative\n");

  return particle_density;
}



LRF_Momentum sample_momentum(default_random_engine& generator, long * acceptances, long * samples, double mass, double sign, double T, double chem)
{
  // sample the local rest frame momentum from a thermal distribution

  double mbar = mass / T;
  double mbar_squared = mbar * mbar;

  // currently the momentum sampler does not work for photons and bosons with nonzero chemical potential
  // so non-equilibrium or electric / strange charge chemical potentials are not considered
  // if(sign == -1.0 && chem != 0.0)
  // {
  //   printf("Error: bosons have chemical potential. Exiting...\n");
  //   exit(-1);
  // }

  double pbar, Ebar, phi_over_2pi, costheta, feq, weight;

  LRF_Momentum pLRF;

  // if(mass == 0.0)
  // {
  //   printf("Error: cannot sample photons with this method. Exiting...\n");
  //   exit(-1);
  // }
  if(mbar < 1.008)
  {
    double weq_max = 1.0; // default value if don't need to rescale weq = exp(p/T) / (exp(E/T) + sign)

    if(mbar < 0.8554 && sign == -1.0) weq_max = pion_thermal_weight_max(mbar, chem);

    double r1, r2, r3, l1, l2, l3;

    while(true)
    {
      *samples = (*samples) + 1;
      // draw (p,phi,costheta) from p^2.exp(-p/T).dp.dphi.dcostheta by sampling (r1,r2,r3)
      r1 = 1.0 - canonical(generator);
      r2 = 1.0 - canonical(generator);
      r3 = 1.0 - canonical(generator);

      l1 = log(r1);
      l2 = log(r2);
      l3 = log(r3);

      pbar = - (l1 + l2 + l3);
      Ebar = sqrt(pbar * pbar  +  mbar_squared);

      feq = 1.0 / (exp(Ebar) + sign);

      weight = feq / weq_max / (r1 * r2 * r3);

      // check if 0 <= weight <= 1
      //if(fabs(weight - 0.5) > 0.5) printf("Sample momentum error: weight = %lf out of bounds\n", weight);

      // check pLRF acceptance
      if(canonical(generator) < weight)
      {
        phi_over_2pi = (l1 + l2) * (l1 + l2) / (pbar * pbar);
        costheta = (l1 - l2) / (l1 + l2);
        break;
      }

    } // rejection loop

  } // sampling pions

  // heavy hadrons (use variable transformation described in LongGang's notes)
  else
  {
    // uniform distributions in costheta
    uniform_real_distribution<double> costheta_distribution(-1.0, nextafter(1.0, numeric_limits<double>::max()));

    // integrated weights
    std::vector<double> K_weight;
    K_weight.resize(3);

    K_weight[0] = mbar_squared; // heavy (sampled_distribution = 0)
    K_weight[1] = 2.0 * mbar;   // moderate (sampled_distribution = 1)
    K_weight[2] = 2.0;          // light (sampled_distribution = 2)

    discrete_distribution<int> K_distribution(K_weight.begin(), K_weight.end());

    double kbar, boltz;  // kinetic energy / T, boltzmann factor

    while(true)
    {
      *samples = (*samples) + 1;

      int sampled_distribution = K_distribution(generator);

      // select distribution to sample from based on integrated weights
      if(sampled_distribution == 0)
      {
        // draw k from exp(-k/T).dk by sampling r1
        // sample direction uniformly
        kbar = - log(1.0 - canonical(generator));
        phi_over_2pi = canonical(generator);
        costheta = costheta_distribution(generator);

      } // distribution 1 (very heavy)
      else if(sampled_distribution == 1)
      {
        // draw (k,phi) from k.exp(-k/T).dk.dphi by sampling (r1,r2)
        // sample costheta uniformly
        double l1 = log(1.0 - canonical(generator));
        double l2 = log(1.0 - canonical(generator));

        kbar = - (l1 + l2);
        phi_over_2pi = - l1 / kbar;
        costheta = costheta_distribution(generator);

      } // distribution 2 (moderately heavy)
      else if(sampled_distribution == 2)
      {
        // draw (k,phi,costheta) from k^2.exp(-k/T).dk.dphi.dcostheta by sampling (r1,r2,r3)
        double l1 = log(1.0 - canonical(generator));
        double l2 = log(1.0 - canonical(generator));
        double l3 = log(1.0 - canonical(generator));

        kbar = - (l1 + l2 + l3);
        phi_over_2pi = (l1 + l2) * (l1 + l2) / (kbar * kbar);
        costheta = (l1 - l2) / (l1 + l2);

      } // distribution 3 (light)
      else
      {
        printf("Error: did not sample any K distribution\n");
        exit(-1);
      }

      Ebar = kbar + mbar;                        // energy / T
      pbar = sqrt(Ebar * Ebar  -  mbar_squared); // momentum magnitude / T

      boltz = exp(Ebar - chem);

      feq = 1.0 / (boltz + sign);

      weight = pbar/Ebar * boltz * feq;

      //if(fabs(weight - 0.5) > 0.5) printf("Sample momemtum error: weight = %f out of bounds\n", weight);

      // check pLRF acceptance
      if(canonical(generator) < weight) break;

    } // rejection loop

  } // sampling heavy hadrons

  *acceptances = (*acceptances) + 1;

  double p = pbar * T;
  double phi = phi_over_2pi * two_pi;
  double sintheta = sqrt(1.0  -  costheta * costheta);    // sin(theta)

  pLRF.E = Ebar * T;
  pLRF.px = p * sintheta * cos(phi);
  pLRF.py = p * sintheta * sin(phi);
  pLRF.pz = p * costheta;
  pLRF.feq = feq;

  return pLRF;

}

LRF_Momentum rescale_momentum(LRF_Momentum pLRF_mod, double mass_squared, double baryon, double pixx, double pixy, double pixz, double piyy, double piyz, double pizz, double Vx, double Vy, double Vz, double shear_mod, double isotropic_scale, double diff_mod, double baryon_enthalpy_ratio)
{
    double E = pLRF_mod.E;
    double px = pLRF_mod.px;
    double py = pLRF_mod.py;
    double pz = pLRF_mod.pz;

    diff_mod *= (E * baryon_enthalpy_ratio  +  baryon);

    LRF_Momentum pLRF;

    // local momentum transformation
    // p_i = A_ij * p_mod_j  +  E_mod * q_i  +  b * T * a_i
    pLRF.px = isotropic_scale * px  +  shear_mod * (pixx * px  +  pixy * py  +  pixz * pz)  +  diff_mod * Vx;
    pLRF.py = isotropic_scale * py  +  shear_mod * (pixy * px  +  piyy * py  +  piyz * pz)  +  diff_mod * Vy;
    pLRF.pz = isotropic_scale * pz  +  shear_mod * (pixz * px  +  piyz * py  +  pizz * pz)  +  diff_mod * Vz;
    pLRF.E = sqrt(mass_squared  +  pLRF.px * pLRF.px  +  pLRF.py * pLRF.py  +  pLRF.pz * pLRF.pz);

    return pLRF;
}

LRF_Momentum rescale_momentum_famod(LRF_Momentum pLRF_mod, double mass_squared, double Bxx, double Bxy, double Bxz, double Byy, double Byz, double Bzz)
{
  double px = pLRF_mod.px;
  double py = pLRF_mod.py;
  double pz = pLRF_mod.pz;

  LRF_Momentum pLRF;

  // local momentum transformation p_i = B_ij . p_mod_j    (maybe i should check commutation)

  pLRF.px = Bxx * px  +  Bxy * py  +  Bxz * pz;
  pLRF.py = Bxy * px  +  Byy * py  +  Byz * pz;
  pLRF.pz = Bxz * px  +  Byz * py  +  Bzz * pz;
  pLRF.E = sqrt(mass_squared  +  pLRF.px * pLRF.px  +  pLRF.py * pLRF.py  +  pLRF.pz * pLRF.pz);

  return pLRF;
}


double EmissionFunctionArray::calculate_total_yield(double * Equilibrium_Density, double * Bulk_Density, double * Diffusion_Density, double *T_fo, double *P_fo, double *E_fo, double *tau_fo, double *ux_fo, double *uy_fo, double *un_fo, double *dat_fo, double *dax_fo, double *day_fo, double *dan_fo, double *pixx_fo, double *pixy_fo, double *pixn_fo, double *piyy_fo, double *piyn_fo, double *bulkPi_fo, double *muB_fo, double *nB_fo, double *Vx_fo, double *Vy_fo, double *Vn_fo, Deltaf_Data * df_data, Gauss_Laguerre * laguerre)
  {
    // estimate the total mean particle yield from the freezeout surface
    // to determine the number of events you want to sample

    int npart = number_of_chosen_particles;

    double Ntot = 0;                      // total particle yield

    //#pragma omp parallel for reduction(+:Ntot)
    for(long icell = 0; icell < FO_length; icell++)
    {
      double tau = tau_fo[icell];         // longitudinal proper time
      double tau2 = tau * tau;

      double dat = dat_fo[icell];         // covariant normal surface vector dsigma_mu
      double dax = dax_fo[icell];
      double day = day_fo[icell];
      double dan = dan_fo[icell];         // dan should be 0 in 2+1d case

      double ux = ux_fo[icell];           // contravariant fluid velocity u^mu
      double uy = uy_fo[icell];           // enforce normalization
      double un = un_fo[icell];           // u^eta (fm^-1)
      double ut = sqrt(1. +  ux * ux  + uy * uy  +  tau2 * un * un);   // u^tau

      double uperp =  sqrt(ux * ux  +  uy * uy);                        // useful expressions
      double utperp = sqrt(1.  +  ux * ux  +  uy * uy);
      double ux2 = ux * ux;
      double uy2 = uy * uy;
      double ut2 = ut * ut;

      double udsigma = ut * dat  +  ux * dax  +  uy * day  +  un * dan; // udotdsigma / eta_weight

      if(udsigma <= 0)
      {
        continue;                         // skip over cells with u.dsigma < 0
      }

      double T = T_fo[icell];             // temperature (GeV)
      double P = P_fo[icell];             // equilibrium pressure (GeV/fm^3)
      double E = E_fo[icell];             // energy density (GeV/fm^3)

      double pitt = 0;                    // contravariant shear stress tensor pi^munu (GeV/fm^3)
      double pitx = 0;                    // enforce orthogonality pi.u = 0
      double pity = 0;                    // and tracelessness Tr(pi) = 0
      double pitn = 0;
      double pixx = 0;
      double pixy = 0;
      double pixn = 0;
      double piyy = 0;
      double piyn = 0;
      double pinn = 0;

      if(INCLUDE_SHEAR_DELTAF)
      {
        pixx = pixx_fo[icell];
        pixy = pixy_fo[icell];
        pixn = pixn_fo[icell];
        piyy = piyy_fo[icell];
        piyn = piyn_fo[icell];
        pinn = (pixx * (ux2 - ut2)  +  piyy * (uy2 - ut2)  +  2. * (pixy * ux * uy  +  tau2 * un * (pixn * ux  +  piyn * uy))) / (tau2 * utperp * utperp);
        pitn = (pixn * ux  +  piyn * uy  +  tau2 * pinn * un) / ut;
        pity = (pixy * ux  +  piyy * uy  +  tau2 * piyn * un) / ut;
        pitx = (pixx * ux  +  pixy * uy  +  tau2 * pixn * un) / ut;
        pitt = (pitx * ux  +  pity * uy  +  tau2 * pitn * un) / ut;
      }

      double bulkPi = 0.;                  // bulk pressure (GeV/fm^3)

      if(INCLUDE_BULK_DELTAF)
      {
        bulkPi = bulkPi_fo[icell];
      }

      double muB = 0.;                     // baryon chemical potential [GeV]
      double nB = 0.;                      // net-baryon density
      double Vt = 0.;                      // contravariant baryon diffusion current V^mu
      double Vx = 0.;
      double Vy = 0.;
      double Vn = 0.;

      if(INCLUDE_BARYON)
      {
        muB = muB_fo[icell];
        nB = nB_fo[icell];

        if(INCLUDE_BARYONDIFF_DELTAF)
        {
          Vx = Vx_fo[icell];
          Vy = Vy_fo[icell];
          Vn = Vn_fo[icell];
          Vt = (Vx * ux  +  Vy * uy  +  tau2 * Vn * un) / ut;     // enforce V.u = 0
        }
      }

      double Vdsigma = Vt * dat  +  Vx * dax  +  Vy * day  +  Vn * dan;   // Vdotdsigma / delta_eta_weight
      double alphaB = muB / T;
      double baryon_enthalpy_ratio = nB / (E + P);

      // regulate bulk pressure if goes out of bounds given
      // by Jonah's feqmod to avoid gsl interpolation errors
      if(DF_MODE == 4)
      {
        double bulkPi_over_Peq_max = df_data->bulkPi_over_Peq_max;

        if(bulkPi <= - P)
        {
          bulkPi = - (1.0 - 1.e-5) * P;
        }
        else if(bulkPi / P >= bulkPi_over_Peq_max)
        {
          bulkPi = P * (bulkPi_over_Peq_max - 1.e-5);
        }
      }

      // evaluate df coefficients
      deltaf_coefficients df = df_data->evaluate_df_coefficients(T, muB, E, P, bulkPi);

      // modified coefficients for PTM and PTB
      double F = df.F;
      double G = df.G;
      double betabulk = df.betabulk;
      double betapi = df.betapi;
      double lambda = df.lambda;
      double z = df.z;
      double delta_z = df.delta_z;

      // milne basis vectors
      Milne_Basis basis_vectors(ut, ux, uy, un, uperp, utperp, tau);
      basis_vectors.test_orthonormality(tau2);

      // surface element class
      Surface_Element_Vector dsigma(dat, dax, day, dan);
      dsigma.boost_dsigma_to_lrf(basis_vectors, ut, ux, uy, un);

      double ds_time = dsigma.dsigmat_LRF;
      double ds_space = dsigma.dsigma_space;

      // shear stress class
      Shear_Stress pimunu(pitt, pitx, pity, pitn, pixx, pixy, pixn, piyy, piyn, pinn);
      pimunu.test_pimunu_orthogonality_and_tracelessness(ut, ux, uy, un, tau2);
      pimunu.boost_pimunu_to_lrf(basis_vectors, tau2);


      // modified temperature / chemical potential and rescaling coefficients
      double T_mod = T;
      double alphaB_mod = alphaB;

      double shear_mod = 0;
      double bulk_mod = 0;

      if(DF_MODE == 3)
      {
        T_mod = T  +  bulkPi * F / betabulk;
        alphaB_mod = alphaB  +  bulkPi * G / betabulk;

        shear_mod = 0.5 / betapi;
        bulk_mod = bulkPi / (3. * betabulk);
      }
      else if(DF_MODE == 4)
      {
        shear_mod = 0.5 / betapi;
        bulk_mod = lambda;
      }

      double detA = compute_detA(pimunu, shear_mod, bulk_mod);

      // determine if feqmod breaks down
      bool feqmod_breaks_down = does_feqmod_breakdown(MASS_PION0, T, F, bulkPi, betabulk, detA, DETA_MIN, z, laguerre, DF_MODE, 0, T, F, betabulk);

      // sum over hadrons
      for(int ipart = 0; ipart < npart; ipart++)
      {
        double equilibrium_density = Equilibrium_Density[ipart];
        double bulk_density = Bulk_Density[ipart];
        double diffusion_density = Diffusion_Density[ipart];

        Ntot += estimate_mean_particle_number(equilibrium_density, bulk_density, diffusion_density, ds_time, ds_space, bulkPi, Vdsigma, z, delta_z, feqmod_breaks_down, DF_MODE);
      }
    } // freezeout cells (icell)

    if(DIMENSION == 2)
    {
      Ntot *= (2.0 * Y_CUT);
    }

    printf("Estimated total particle yield = %ld particles\n", (long)Ntot);

    return Ntot;
  }

void EmissionFunctionArray::sample_dN_pTdpTdphidy(double *Mass, double *Sign, double *Degeneracy, double *Baryon, int *MCID, double *Equilibrium_Density, double *Bulk_Density, double *Diffusion_Density, double *T_fo, double *P_fo, double *E_fo, double *tau_fo, double *x_fo, double *y_fo, double *eta_fo, double *ux_fo, double *uy_fo, double *un_fo, double *dat_fo, double *dax_fo, double *day_fo, double *dan_fo, double *pixx_fo, double *pixy_fo, double *pixn_fo, double *piyy_fo, double *piyn_fo, double *bulkPi_fo, double *muB_fo, double *nB_fo, double *Vx_fo, double *Vy_fo, double *Vn_fo, Deltaf_Data *df_data, Gauss_Laguerre * laguerre, Gauss_Legendre * legendre)
  {
    int npart = number_of_chosen_particles;

    double y_max = 0.5;                 // effective volume extension by 2.y_max
    if(DIMENSION == 2) y_max = Y_CUT;   // default value is 2.y_max = 1 (for 3+1d)

    // set seed
    unsigned seed;
    if (SAMPLER_SEED < 0) seed = chrono::system_clock::now().time_since_epoch().count();
    else seed = SAMPLER_SEED;

    default_random_engine generator_poisson(seed);
    default_random_engine generator_type(seed + 10000);
    default_random_engine generator_momentum(seed + 20000);
    //default_random_engine generator_keep(seed + 30000);
    default_random_engine generator_rapidity(seed + 40000);

    // get average temperature (for fast mode)
    Plasma QGP;
    QGP.load_thermodynamic_averages();
    const double Tavg = QGP.temperature;
    const double muBavg = QGP.baryon_chemical_potential;

    double F_avg, betabulk_avg; // for feqmod breakdown (compute Tavg pion density)

    if(FAST)
    {
      printf("Using fast mode (Tavg, muBavg) = (%lf, %lf)\n", Tavg, muBavg);
      if(DF_MODE == 3)
      {
        deltaf_coefficients df_avg = df_data->evaluate_df_coefficients(Tavg, muBavg, 0.0, 0.0, 0.0);
        F_avg = df_avg.F;
        betabulk_avg = df_avg.betabulk;
      }
    }

    // for benchmarking momentum sampling efficiency
    long acceptances = 0;
    long samples = 0;

    // loop over all freezeout cells
    for(long icell = 0; icell < FO_length; icell++)
    {
      double tau = tau_fo[icell];         // freezeout cell coordinates
      double tau2 = tau * tau;
      double x = x_fo[icell];
      double y = y_fo[icell];
      double eta = eta_fo[icell];

      double sinheta = sinh(eta);
      double cosheta = sqrt(1.0  +  sinheta * sinheta);

      double dat = dat_fo[icell];         // covariant normal surface vector dsigma_mu
      double dax = dax_fo[icell];
      double day = day_fo[icell];
      double dan = dan_fo[icell];         // dan should be 0 in 2+1d case

      double ux = ux_fo[icell];           // contravariant fluid velocity u^mu
      double uy = uy_fo[icell];           // enforce normalization u.u = 1
      double un = un_fo[icell];           // u^eta (fm^-1)
      double ut = sqrt(1.0  +  ux * ux  +  uy * uy  +  tau2 * un * un); // u^tau

      double udsigma = ut * dat  +  ux * dax  +  uy * day  +  un * dan; // udotdsigma / eta_weight

      if(udsigma <= 0.0) continue;        // skip over cells with u.dsigma < 0

      double ut2 = ut * ut;               // useful expressions
      double ux2 = ux * ux;
      double uy2 = uy * uy;
      double uperp =  sqrt(ux * ux  +  uy * uy);
      double utperp = sqrt(1.0  +  ux * ux  +  uy * uy);

      double T = T_fo[icell];             // temperature (GeV)
      double P = P_fo[icell];             // pressure (GeV/fm^3)
      double Energy = E_fo[icell];        // energy density (GeV/fm^3)

      double pitt = 0.0;                  // contravariant shear stress tensor pi^munu (GeV/fm^3)
      double pitx = 0.0;                  // enforce orthogonality pi.u = 0
      double pity = 0.0;                  // and tracelessness Tr(pi) = 0
      double pitn = 0.0;
      double pixx = 0.0;
      double pixy = 0.0;
      double pixn = 0.0;
      double piyy = 0.0;
      double piyn = 0.0;
      double pinn = 0.0;

      if(INCLUDE_SHEAR_DELTAF)
      {
        pixx = pixx_fo[icell];
        pixy = pixy_fo[icell];
        pixn = pixn_fo[icell];
        piyy = piyy_fo[icell];
        piyn = piyn_fo[icell];
        pinn = (pixx * (ux2 - ut2)  +  piyy * (uy2 - ut2)  +  2.0 * (pixy * ux * uy  +  tau2 * un * (pixn * ux  +  piyn * uy))) / (tau2 * utperp * utperp);
        pitn = (pixn * ux  +  piyn * uy  +  tau2 * pinn * un) / ut;
        pity = (pixy * ux  +  piyy * uy  +  tau2 * piyn * un) / ut;
        pitx = (pixx * ux  +  pixy * uy  +  tau2 * pixn * un) / ut;
        pitt = (pitx * ux  +  pity * uy  +  tau2 * pitn * un) / ut;
      }

      double bulkPi = 0.0;                // bulk pressure (GeV/fm^3)

      if(INCLUDE_BULK_DELTAF) bulkPi = bulkPi_fo[icell];

      double muB = 0.0;
      double alphaB = 0.0;
      double nB = 0.0;
      double Vt = 0.0;                    // contravariant net baryon diffusion current V^mu
      double Vx = 0.0;                    // enforce orthogonality
      double Vy = 0.0;
      double Vn = 0.0;
      double Vdsigma = 0.0;               // Vdotdsigma
      double baryon_enthalpy_ratio = 0.0;

      if(INCLUDE_BARYON)
      {
        muB = muB_fo[icell];
        nB = nB_fo[icell];
        if(INCLUDE_BARYONDIFF_DELTAF){
          Vx = Vx_fo[icell];
          Vy = Vy_fo[icell];
          Vn = Vn_fo[icell];
          Vt = (Vx * ux  +  Vy * uy  +  tau2 * Vn * un) / ut;
          Vdsigma = Vt * dat  +  Vx * dax  +  Vy * day  +  Vn * dan;
        }
        
        alphaB = muB / T;
        baryon_enthalpy_ratio = nB / (Energy + P);
      }

      // regulate bulk pressure if goes out of bounds given
      // by Jonah's feqmod to avoid gsl interpolation errors
      if(DF_MODE == 4)
      {
        double bulkPi_over_Peq_max = df_data->bulkPi_over_Peq_max;

        if(bulkPi <= - P) bulkPi = - (1.0 - 1.e-5) * P;
        else if(bulkPi / P >= bulkPi_over_Peq_max) bulkPi = P * (bulkPi_over_Peq_max - 1.e-5);
      }

      deltaf_coefficients df = df_data->evaluate_df_coefficients(T, muB, Energy, P, bulkPi);

      // df coefficients
      double c0 = df.c0;
      double c1 = df.c1;
      double c2 = df.c2;
      double c3 = df.c3;
      double c4 = df.c4;
      double shear14_coeff = df.shear14_coeff;

      double F = df.F;
      double G = df.G;
      double betabulk = df.betabulk;
      double betaV = df.betaV;
      double betapi = df.betapi;

      double lambda = df.lambda;
      double z = df.z;
      double delta_lambda = df.delta_lambda;
      double delta_z = df.delta_z;

      // useful expressions
      double c0_minus_c2 = c0 - c2;
      double fourc2_minus_c0 = 4.0*c2 - c0;

      double two_betapi_T = 2.0 * betapi * T;
      double three_T = 3.0 * T;
      double F_over_T2 = F / (T * T);
      double bulkPi_over_betabulk = bulkPi / betabulk;

      double delta_z_minus_three_delta_lambda = delta_z - 3.0 * delta_lambda;
      double delta_lambda_over_T = delta_lambda / T;

      // milne basis class
      Milne_Basis basis_vectors(ut, ux, uy, un, uperp, utperp, tau);
      basis_vectors.test_orthonormality(tau2);

      // dsigma / eta_weight class
      Surface_Element_Vector dsigma(dat, dax, day, dan);
      dsigma.boost_dsigma_to_lrf(basis_vectors, ut, ux, uy, un);
      dsigma.compute_dsigma_magnitude();

      // LRF components
      double dst = dsigma.dsigmat_LRF;
      double dsx = dsigma.dsigmax_LRF;
      double dsy = dsigma.dsigmay_LRF;
      double dsz = dsigma.dsigmaz_LRF;
      double ds_max = dsigma.dsigma_magnitude;

      // shear stress class
      Shear_Stress pimunu(pitt, pitx, pity, pitn, pixx, pixy, pixn, piyy, piyn, pinn);
      pimunu.test_pimunu_orthogonality_and_tracelessness(ut, ux, uy, un, tau2);
      pimunu.boost_pimunu_to_lrf(basis_vectors, tau2);

      double pixx_LRF = pimunu.pixx_LRF;
      double pixy_LRF = pimunu.pixy_LRF;
      double pixz_LRF = pimunu.pixz_LRF;
      double piyy_LRF = pimunu.piyy_LRF;
      double piyz_LRF = pimunu.piyz_LRF;
      double pizz_LRF = pimunu.pizz_LRF;

      // baryon diffusion class
      Baryon_Diffusion Vmu(Vt, Vx, Vy, Vn);
      Vmu.test_Vmu_orthogonality(ut, ux, uy, un, tau2);
      Vmu.boost_Vmu_to_lrf(basis_vectors, tau2);

      double Vx_LRF = Vmu.Vx_LRF;
      double Vy_LRF = Vmu.Vy_LRF;
      double Vz_LRF = Vmu.Vz_LRF;

      // modified temperature / chemical potential and rescaling coefficients
      double T_mod = T;
      double alphaB_mod = alphaB;

      double shear_mod, bulk_mod, diff_mod;

      if(DF_MODE == 3)
      {
        T_mod = T  +  bulkPi * F / betabulk;
        alphaB_mod = alphaB  +  bulkPi * G / betabulk;

        shear_mod = 0.5 / betapi;
        bulk_mod = bulkPi / (3.0 * betabulk);
        diff_mod = T / betaV;
      }
      else if(DF_MODE == 4)
      {
        shear_mod = 0.5 / betapi;
        bulk_mod = lambda;
        diff_mod = 0.0;
      }
      double isotropic_scale = 1.0 + bulk_mod;

      double detA = compute_detA(pimunu, shear_mod, bulk_mod);

      // determine if feqmod breaks down
      bool feqmod_breaks_down = does_feqmod_breakdown(MASS_PION0, T, F, bulkPi, betabulk, detA, DETA_MIN, z, laguerre, DF_MODE, FAST, Tavg, F_avg, betabulk_avg);

      // discrete number fraction of each species
      std::vector<double> dn_list;
      dn_list.resize(npart);

      // total mean number of hadrons emitted from freezeout
      // cell of max volume (volume also scaled by 2.y_max)
      double dn_tot = 0.0;

      if(FAST)
      {
        for(int ipart = 0; ipart < npart; ipart++)
        {
          double equilibrium_density = Equilibrium_Density[ipart];
          double bulk_density = Bulk_Density[ipart];

          dn_list[ipart] = fast_max_particle_number(equilibrium_density, bulk_density, bulkPi, z, feqmod_breaks_down, DF_MODE);
          dn_tot += dn_list[ipart];
        }
      }
      else
      {
        double neq_fact = T * T * T / two_pi2_hbarC3;
        double J20_fact = T * neq_fact;

        for(int ipart = 0; ipart < npart; ipart++)
        {
          double mass = Mass[ipart];
          double mbar = mass / T;
          double degeneracy = Degeneracy[ipart];
          double sign = Sign[ipart];
          double baryon = Baryon[ipart];

          dn_list[ipart] = max_particle_number(mbar, degeneracy, sign, baryon, T, alphaB, bulkPi, df, feqmod_breaks_down, laguerre, DF_MODE, INCLUDE_BARYON, neq_fact, J20_fact);
          dn_tot += dn_list[ipart];
        }
      }

      if(dn_tot <= 0.0) continue;

      dn_tot *= (2.0 * y_max * ds_max);                      // multiply by the volume


      // construct discrete probability distribution for particle types (weight[ipart] ~ dn_list[ipart] / dn_tot)
      std::discrete_distribution<int> particle_type(dn_list.begin(), dn_list.end());

      // construct poisson probability distribution for number of hadrons
      std::poisson_distribution<int> poisson_hadrons(dn_tot);

      // sample events for each FO cell
      for(long ievent = 0; ievent < Nevents; ievent++)
      {
        int N_hadrons = poisson_hadrons(generator_poisson);   // sample total number of hadrons in FO cell

        for(int n = 0; n < N_hadrons; n++)
        {
          int chosen_index = particle_type(generator_type);   // chosen index of sampled particle type

          double mass = Mass[chosen_index];                   // mass of sampled particle in GeV
          double mass_squared = mass * mass;
          double sign = Sign[chosen_index];                   // quantum statistics sign
          double baryon = Baryon[chosen_index];               // baryon number
          double chem = baryon * alphaB;
          double chem_mod = baryon * alphaB_mod;
          int mcid = MCID[chosen_index];                      // mc_id

          LRF_Momentum pLRF;                                  // local rest frame momentum
          double w_visc = 1.0;                                // viscous weight
          double w_flux;                                      // flux weight

          // sample the local rest frame momentum
          // and compute viscous weight
          switch(DF_MODE)
          {
            case 1: // 14 moment
            {
              pLRF = sample_momentum(generator_momentum, &acceptances, &samples, mass, sign, T, chem);

              double E = pLRF.E;
              double px = pLRF.px;
              double py = pLRF.py;
              double pz = pLRF.pz;
              double feq = pLRF.feq;

              double feqbar = 1.0 - sign * feq;
              double pimunu_pmu_pnu = px*px*pixx_LRF + py*py*piyy_LRF + pz*pz*pizz_LRF + 2.*(px*py*pixy_LRF + px*pz*pixz_LRF + py*pz*piyz_LRF);
              double Vmu_pmu = - (px*Vx_LRF + py*Vy_LRF + pz*Vz_LRF);

              double df_shear = pimunu_pmu_pnu / shear14_coeff;
              double df_bulk = (c0_minus_c2 * mass_squared  +  (baryon * c1  +  fourc2_minus_c0 * E) * E) * bulkPi;
              double df_diff = (baryon * c3  +  c4 * E) * Vmu_pmu;

              double df_reg = max(-1.0, min(1.0, feqbar * (df_shear + df_bulk + df_diff)));

              w_visc = (1.0 + df_reg) / 2.0;
              w_flux = max(0.0, E * dst  -  px * dsx  -  py * dsy  -  pz * dsz) / (E * ds_max);

              break;
            }
            case 2: // Chapman Enskog
            {
              chapman_enskog:

              pLRF = sample_momentum(generator_momentum, &acceptances, &samples, mass, sign, T, chem);

              double E = pLRF.E;
              double px = pLRF.px;
              double py = pLRF.py;
              double pz = pLRF.pz;
              double feq = pLRF.feq;

              double feqbar = 1.0 - sign * feq;
              double pimunu_pmu_pnu = px*px*pixx_LRF + py*py*piyy_LRF + pz*pz*pizz_LRF + 2.*(px*py*pixy_LRF + px*pz*pixz_LRF + py*pz*piyz_LRF);
              double Vmu_pmu = - (px*Vx_LRF + py*Vy_LRF + pz*Vz_LRF);

              double df_shear = pimunu_pmu_pnu / (two_betapi_T * E);
              double df_bulk = (baryon * G  +  F_over_T2 * E  +  (E - mass_squared / E) / three_T) * bulkPi_over_betabulk;
              double df_diff = (baryon_enthalpy_ratio  -  baryon / E) * Vmu_pmu / betaV;

              double df_reg = max(-1.0, min(1.0, feqbar * (df_shear + df_bulk + df_diff)));

              w_visc = (1.0 + df_reg) / 2.0;
              w_flux = max(0.0, E * dst  -  px * dsx  -  py * dsy  -  pz * dsz) / (E * ds_max);

              break;
            }
            case 3: // Modified (Mike)
            {
              if(feqmod_breaks_down) goto chapman_enskog;

              pLRF = sample_momentum(generator_momentum, &acceptances, &samples, mass, sign, T_mod, chem_mod);
              pLRF = rescale_momentum(pLRF, mass_squared, baryon, pixx_LRF, pixy_LRF, pixz_LRF, piyy_LRF, piyz_LRF, pizz_LRF, Vx_LRF, Vy_LRF, Vz_LRF, shear_mod, isotropic_scale, diff_mod, baryon_enthalpy_ratio);

              double E = pLRF.E;
              double px = pLRF.px;
              double py = pLRF.py;
              double pz = pLRF.pz;
              w_flux = max(0.0, E * dst  -  px * dsx  -  py * dsy  -  pz * dsz) / (E * ds_max);

              break;
            }
            case 4: // Modified (Jonah)
            {
              pLRF = sample_momentum(generator_momentum, &acceptances, &samples, mass, sign, T, 0.0);

              if(!feqmod_breaks_down)
              {
                pLRF = rescale_momentum(pLRF, mass_squared, 0.0, pixx_LRF, pixy_LRF, pixz_LRF, piyy_LRF, piyz_LRF, pizz_LRF, 0.0, 0.0, 0.0, shear_mod, isotropic_scale, 0.0, 0.0);

                double E = pLRF.E;
                double px = pLRF.px;
                double py = pLRF.py;
                double pz = pLRF.pz;
                w_flux = max(0.0, E * dst  -  px * dsx  -  py * dsy  -  pz * dsz) / (E * ds_max);
              }
              else
              {
                double E = pLRF.E;
                double px = pLRF.px;
                double py = pLRF.py;
                double pz = pLRF.pz;
                double feq = pLRF.feq;

                double feqbar = 1.0 - sign * feq;
                double pimunu_pmu_pnu = px*px*pixx_LRF + py*py*piyy_LRF + pz*pz*pizz_LRF + 2.*(px*py*pixy_LRF + px*pz*pixz_LRF + py*pz*piyz_LRF);

                double df_shear = feqbar * pimunu_pmu_pnu / (two_betapi_T * E);
                double df_bulk = delta_z_minus_three_delta_lambda  +  feqbar * delta_lambda_over_T * (E  -  mass_squared / E);

                double df_reg = max(-1.0, min(1.0, df_shear + df_bulk));
                w_visc = (1.0 + df_reg) / 2.0;
                w_flux = max(0.0, E * dst  -  px * dsx  -  py * dsy  -  pz * dsz) / (E * ds_max);
              }

              break;
            }
            default:
            {
              printf("\nError: for viscous hydro momentum sampling please set df_mode = (1,2,3,4)\n");
              exit(-1);
            }
          }

          // add particle
          if(canonical(generator_momentum) < (w_flux * w_visc))
          {
            // lab frame momentum
            Lab_Momentum pLab(pLRF);
            pLab.boost_pLRF_to_lab_frame(basis_vectors, ut, ux, uy, un);

            // new sampled particle info
            Sampled_Particle new_particle;

            new_particle.chosen_index = chosen_index;
            new_particle.mcID = mcid;
            new_particle.tau = tau;
            new_particle.x = x;
            new_particle.y = y;
            new_particle.mass = mass;
            new_particle.px = pLab.px;
            new_particle.py = pLab.py;

            double E, pz, rapidity;

            if(DIMENSION == 2)
            {
              rapidity = uniform_rapidity_distribution(generator_rapidity, y_max);

              double sinhy = sinh(rapidity);
              double coshy = sqrt(1.0 + sinhy * sinhy);

              double ptau = pLab.ptau;
              double tau_pn = tau * pLab.pn;
              double mT = sqrt(ptau*ptau - tau_pn*tau_pn);

              sinheta = (ptau*sinhy - tau_pn*coshy) / mT;
              eta = asinh(sinheta);
              cosheta = sqrt(1.0 + sinheta * sinheta);

              pz = mT * sinhy;
              E = mT * coshy;
                //cout << eta - (rapidity - atanh(tau_pn / ptau)) << endl;
                //cout << rapidity << "\t\t" << eta - (rapidity - asinh(tau_pn/mT)) << endl;
            }
            else
            {
              pz = tau * pLab.pn * cosheta  +  pLab.ptau * sinheta;
              E = sqrt(mass_squared +  pLab.px * pLab.px  +  pLab.py * pLab.py  +  pz * pz);
              rapidity = 0.5 * log((E + pz) / (E - pz));
            }

            new_particle.eta = eta;
            new_particle.t = tau * cosheta;
            new_particle.z = tau * sinheta;
            new_particle.E = E;
            new_particle.pz = pz;
            
            if(TEST_SAMPLER)
            {
              // bin the distributions (avoids memory bottleneck)
              sample_dN_dy(chosen_index, rapidity);
              sample_dN_deta(chosen_index, eta);
              sample_dN_2pipTdpTdy(chosen_index, rapidity, pLab.px, pLab.py);
              sample_dN_dphipdy(chosen_index, rapidity, pLab.px, pLab.py);
              sample_vn(chosen_index, rapidity, pLab.px, pLab.py);
              sample_dN_dX(chosen_index, tau, x, y);
            } else {
              //CAREFUL push_back is not a thread-safe operation
              //how should we modify for GPU version?
              #pragma omp critical
              particle_event_list[ievent].push_back(new_particle);
            }
          } // add sampled particle to event list
        } // sampled hadrons (n)
      } // sampled events (ievent)
    } // freezeout cells (icell)
    printf("Momentum sampling efficiency = %f %%\n", (float)(100.0 * (double)acceptances / (double)samples));
}



void EmissionFunctionArray::sample_dN_pTdpTdphidy_famod(double *Mass, double *Sign, double *Degeneracy, double *Baryon, int *MCID, double *T_fo, double *P_fo, double *E_fo, double *tau_fo, double *x_fo, double *y_fo, double *eta_fo, double *ux_fo, double *uy_fo, double *un_fo, double *dat_fo, double *dax_fo, double *day_fo, double *dan_fo, double *pixx_fo, double *pixy_fo, double *pixn_fo, double *piyy_fo, double *piyn_fo, double *bulkPi_fo, double *muB_fo, double *nB_fo, double *Vx_fo, double *Vy_fo, double *Vn_fo, int Nparticles, double *Mass_PDG, double *Sign_PDG, double *Degeneracy_PDG, double *Baryon_PDG)
{
  int npart = number_of_chosen_particles;

  double y_max = 0.5;                 // for rapidity volume extension factor 2.y_max (default 2.y_max = 1 for 3+1d)

  if(DIMENSION == 2)
  {
    y_max = Y_CUT;                    // volume extension factor = 2.y_cut for 2+1d surface
  }

  unsigned seed;                      // set the seed

  if(SAMPLER_SEED < 0)
  {
    seed = chrono::system_clock::now().time_since_epoch().count();
  }
  else
  {
    seed = SAMPLER_SEED;
  }

  default_random_engine generator_poisson(seed);            // initialize random number engines for each distribution
  default_random_engine generator_type(seed + 10000);
  default_random_engine generator_momentum(seed + 20000);
  default_random_engine generator_rapidity(seed + 40000);

  double detB_min = DETA_MIN;         // default value for minimum detB = detC . detA

  double lambda_prev;                 // for tracking reconstruction of anisotropic variables
  double aT_prev;
  double aL_prev;
  bool previous_reconstruction_success = false;
  long reconstruction_fail = 0;
  long plpt_negative = 0;

  long acceptances = 0;               // for benchmarking momentum sampling efficiency
  long samples = 0;


  for(long icell = 0; icell < FO_length; icell++)  // loop over freezeout cells
  {
    // freezeout cell info
    double tau = tau_fo[icell];         // longitudinal proper time
    double tau2 = tau * tau;
    double x = x_fo[icell];             // x and y
    double y = y_fo[icell];
    double eta = eta_fo[icell];         // spacetime rapidity

    double sinheta = sinh(eta);
    double cosheta = sqrt(1.  +  sinheta * sinheta);

    double dat = dat_fo[icell];         // covariant normal surface vector
    double dax = dax_fo[icell];
    double day = day_fo[icell];
    double dan = dan_fo[icell];

    double ux = ux_fo[icell];           // contravariant fluid velocity
    double uy = uy_fo[icell];           // enforce normalization u.u = 1
    double un = un_fo[icell];
    double ut = sqrt(1.  +  ux * ux  +  uy * uy  +  tau2 * un * un);

    if(ut * dat  +  ux * dax  +  uy * day  +  un * dan <= 0)
    {
      continue;                         // skip cells with u.dsigma < 0
    }

    double ut2 = ut * ut;               // useful expressions
    double ux2 = ux * ux;
    double uy2 = uy * uy;
    double uperp = sqrt(ux * ux  +  uy * uy);
    double utperp = sqrt(1.  +   ux * ux  +  uy * uy);

    double T = T_fo[icell];             // temperature [GeV]
    double P = P_fo[icell];             // equilibrium pressure [GeV/fm^3]
    double E = E_fo[icell];             // energy density [GeV/fm^3]

    double pixx = pixx_fo[icell];       // contravariant standard shear stress tensor pi^\munu
    double pixy = pixy_fo[icell];       // reconstruct remaining components to enforce
    double pixn = pixn_fo[icell];       // orthogonality and tracelessness pi.u = Tr(pi) = 0
    double piyy = piyy_fo[icell];
    double piyn = piyn_fo[icell];

    double pinn = (pixx * (ux2 - ut2)  +  piyy * (uy2 - ut2)  +  2. * (pixy * ux * uy  +  tau2 * un * (pixn * ux  +  piyn * uy))) / (tau2 * utperp * utperp);
    double pitn = (pixn * ux  +  piyn * uy  +  tau2 * pinn * un) / ut;
    double pity = (pixy * ux  +  piyy * uy  +  tau2 * piyn * un) / ut;
    double pitx = (pixx * ux  +  pixy * uy  +  tau2 * pixn * un) / ut;
    double pitt = (pitx * ux  +  pity * uy  +  tau2 * pitn * un) / ut;

    double bulkPi = bulkPi_fo[icell];   // bulk pressure [GeV/fm^3]

    double muB = 0.;                     // baryon chemical potential [GeV]
    double Vt = 0.;                      // contravariant net baryon diffusion V^mu
    double Vx = 0.;                      // enforce orthogonality V.u = 0
    double Vy = 0.;
    double Vn = 0.;

    if(INCLUDE_BARYON)
    {
      muB = muB_fo[icell];

      if(INCLUDE_BARYONDIFF_DELTAF)     // todo: use these if-statement brackets in above viscous sampler
      {
        Vx = Vx_fo[icell];              // baryon diffusion not included in famod yet
        Vy = Vy_fo[icell];
        Vn = Vn_fo[icell];
        Vt = (Vx * ux  +  Vy * uy  +  tau2 * Vn * un) / ut;
      }
    }

    double alphaB = muB / T;            // muB / T


    // milne basis vector components
    Milne_Basis basis_vectors(ut, ux, uy, un, uperp, utperp, tau);
    basis_vectors.test_orthonormality(tau2);

    double Xt = basis_vectors.Xt;
    double Xx = basis_vectors.Xx;
    double Xy = basis_vectors.Xy;
    double Xn = basis_vectors.Xn;

    double Yx = basis_vectors.Yx;
    double Yy = basis_vectors.Yy;

    double Zt = basis_vectors.Zt;
    double Zn = basis_vectors.Zn;


    // dsigma / eta_weight class
    Surface_Element_Vector dsigma(dat, dax, day, dan);
    dsigma.boost_dsigma_to_lrf(basis_vectors, ut, ux, uy, un);
    dsigma.compute_dsigma_magnitude();

    double dst = dsigma.dsigmat_LRF;      // dsigma LRF components
    double dsx = dsigma.dsigmax_LRF;
    double dsy = dsigma.dsigmay_LRF;
    double dsz = dsigma.dsigmaz_LRF;
    double ds_max = dsigma.dsigma_magnitude;


    // compute shear stress in LRF
    Shear_Stress pimunu(pitt, pitx, pity, pitn, pixx, pixy, pixn, piyy, piyn, pinn);
    pimunu.test_pimunu_orthogonality_and_tracelessness(ut, ux, uy, un, tau2);
    pimunu.boost_pimunu_to_lrf(basis_vectors, tau2);

    double pixx_LRF = pimunu.pixx_LRF;      // standard pi^munu LRF components
    double pixy_LRF = pimunu.pixy_LRF;
    double pixz_LRF = pimunu.pixz_LRF;
    double piyy_LRF = pimunu.piyy_LRF;
    double piyz_LRF = pimunu.piyz_LRF;
    double pizz_LRF = pimunu.pizz_LRF;


    Baryon_Diffusion Vmu(Vt, Vx, Vy, Vn);   // compute baryon diffusion in LRF
    Vmu.test_Vmu_orthogonality(ut, ux, uy, un, tau2);
    Vmu.boost_Vmu_to_lrf(basis_vectors, tau2);

    double Vx_LRF = Vmu.Vx_LRF;             // standard V^\mu LRF components
    double Vy_LRF = Vmu.Vy_LRF;
    double Vz_LRF = Vmu.Vz_LRF;


    // anisotropic hydrodynamic variables
    double pl = P + bulkPi + pizz_LRF;      // longitudinal pressure
    double pt = P + bulkPi - pizz_LRF/2.;   // transverse pressure

    double piTxx_LRF = 0.;                   // piperp^\munu LRF components
    double piTxy_LRF = 0.;
    double piTyy_LRF = 0.;

    double WTzx_LRF = 0.;                    // Wperpz^\mu LRF components
    double WTzy_LRF = 0.;

    if(INCLUDE_SHEAR_DELTAF)                // include residual shear corrections
    {
      piTxx_LRF = (pixx_LRF - piyy_LRF) / 2.;
      piTxy_LRF = pixy_LRF;
      piTyy_LRF = -(piTxx_LRF);

      WTzx_LRF = pixz_LRF;
      WTzy_LRF = piyz_LRF;
    }


    // default initial guess for anisotropic variables

    double lambda = T;                      // effective temperature
    double aT = 1;                          // transverse momentum scale
    double aL = 1;                          // longitudinal momentum scale
    double upsilonB = alphaB;               // effective chemical potential (muB_tilde / lambda) (not reconstructed atm)

    bool fa_famod_breaks_down = false;      // f = famod by default, if true use f = feq instead
    Nparticles = (int)fmin(320, Nparticles);// include most (not all) hadrons to avoid spurious convergence in root solver (saves time)

    if(pl < 0 || pt < 0)                    // don't bother reconstructing anisotropic variables
    {
      fa_famod_breaks_down = true;          // fa breaks down (and so will famod)
      plpt_negative++;
    }
    else                                    // reconstruct anisotropic variables
    {
      if(previous_reconstruction_success)
      {
        lambda = lambda_prev;               // use previous values as initial guess (if last reconstruction succeeded)
        aT = aT_prev;
        aL = aL_prev;
      }

      // this function will need updating to include chemical potential

      aniso_variables X_aniso = find_anisotropic_variables(E, pl, pt, lambda, aT, aL, Nparticles, Mass_PDG, Sign_PDG, Degeneracy_PDG, Baryon_PDG);

      if(X_aniso.did_not_find_solution)
      {
        if(previous_reconstruction_success)
        {
          lambda = T;                         // try again, this time with equilibrium initial guess (in case reconstruction attempt fails)
          aT = 1;
          aL = 1;

          X_aniso = find_anisotropic_variables(E, pl, pt, lambda, aT, aL, Nparticles, Mass_PDG, Sign_PDG, Degeneracy_PDG, Baryon_PDG);

          if(X_aniso.did_not_find_solution)
          {
            fa_famod_breaks_down = true;      // fa breaks down (and so will famod)
            previous_reconstruction_success = false;
            reconstruction_fail++;
          }
          else
          {
            lambda = X_aniso.lambda;          // get the solution (reconstruction was successful)
            aT = X_aniso.aT;
            aL = X_aniso.aL;

            lambda_prev = lambda;             // set initial guess for next reconstruction
            aT_prev = aT;
            aL_prev = aL;

            previous_reconstruction_success = true;
          }
        }
        else
        {
          fa_famod_breaks_down = true;      // fa breaks down (and so will famod)
          previous_reconstruction_success = false;
          reconstruction_fail++;
        }
      }
      else
      {
        lambda = X_aniso.lambda;            // get the solution (reconstruction was successful)
        aT = X_aniso.aT;
        aL = X_aniso.aL;

        lambda_prev = lambda;               // set initial guess for next reconstruction
        aT_prev = aT;
        aL_prev = aL;

        previous_reconstruction_success = true;
      }
    }


    // compute famod coefficients
    famod_coefficient famod = compute_famod_coefficient(lambda, aT, aL, Nparticles, Mass_PDG, Sign_PDG, Degeneracy_PDG, Baryon_PDG);

    double betapiperp = famod.betapiperp;     // beta_{pi,perp}
    double betaWperp = famod.betaWperp;       // beta_{W,perp}

    double shear_coeff = 0.5 / betapiperp;    // 1 / (2.betapiperp)
    double diff_coeff = 1. / betaWperp;       // 1 / (betaWperp)


    // leading order deformation matrix Aij (diagonal)
    double Axx = aT;
    double Ayy = aT;
    double Azz = aL;

    double detA = Axx * Ayy * Azz;


    // residual shear deformation matrix Cij (asymmetric)
    double Cxx = 1.  +  shear_coeff * piTxx_LRF;
    double Cxy = shear_coeff * piTxy_LRF;
    double Cxz = diff_coeff * WTzx_LRF * aT / (aT + aL);

    double Cyx = Cxy;
    double Cyy = 1.  +  shear_coeff * piTyy_LRF;
    double Cyz = diff_coeff * WTzy_LRF * aT / (aT + aL);

    double Czx = diff_coeff * WTzx_LRF * aL / (aT + aL);
    double Czy = diff_coeff * WTzy_LRF * aL / (aT + aL);
    double Czz = 1.;

    double detC = Cxx * (Cyy * Czz  -  Cyz * Czy)  -  Cxy * (Cyx * Czz  -  Cyz * Czx)  +  Cxz * (Cyx * Czy  -  Cyy * Czx);


    // total momentum transformation matrix Bij = Cik.Akj (symmetric)
    double Bxx = Axx  +  aT * shear_coeff * piTxx_LRF;
    double Bxy = aT * shear_coeff * piTxy_LRF;
    double Bxz = diff_coeff * WTzx_LRF * aT * aL / (aT + aL);

    double Byx = Bxy;
    double Byy = Ayy  +  aT * shear_coeff * piTyy_LRF;
    double Byz = diff_coeff * WTzy_LRF * aT * aL / (aT + aL);

    double Bzx = Bxz;
    double Bzy = Byz;
    double Bzz = Azz;

    double detB = detC * detA;

    if(detB <= detB_min)
    {
      fa_famod_breaks_down = true;
    }

    if(fa_famod_breaks_down)
    {
      Bxx = 1;  Bxy = 0;  Bxz = 0;                        // set to identity matrix for feq sampling
                Byy = 1;  Byz = 0;
                          Bzz = 1;
    }


    std::vector<double> dn_list;                          // discrete number fraction of each species
    dn_list.resize(npart);

    double lambda3 = lambda * lambda * lambda;
    double na_fact = lambda3 * detA / two_pi2_hbarC3;     // prefactor in na

    double dn_tot = 0;                                    // total mean number of hadrons emitted from freezeout cell of max volume

    // compute anisotropic particle densities na
    for(int ipart = 0; ipart < npart; ipart++)
    {
      double mass = Mass[ipart];
      double degeneracy = Degeneracy[ipart];
      double sign = Sign[ipart];
      double baryon = Baryon[ipart];

      double mbar = mass / lambda;
      double mbar2 = mbar * mbar;
      double chem = baryon * upsilonB;                    // should be zero atm

      double I_100 = 0;                                   // anisotropic integral

      for(int igauss = 0; igauss < pbar_pts; igauss++)    // radial momentum integration (gauss-laguerre)
      {
        double pbar =     pbar_root_a1[igauss];           // pbar roots and weights for a = 1 (a = n + s)
        double weight = pbar_weight_a1[igauss];

        double Ebar = sqrt(pbar * pbar  +  mbar2);        // E / lambda

        I_100 += pbar * weight * exp(pbar) / (exp(Ebar + chem) + sign);
      }

      dn_list[ipart] = degeneracy * na_fact * I_100;      // multiply by degeneracy and prefactor

      dn_tot += dn_list[ipart];                           // append to total
    }

    if(dn_tot <= 0)
    {
      continue;
    }

    dn_tot *= (2. * y_max * ds_max);  // multiply max volume element (and 2.y_max factor if 2+1d cells)


    // construct discrete probability distribution for particle types (weight[ipart] ~ dn_list[ipart] / dn_tot)
    std::discrete_distribution<int> particle_type(dn_list.begin(), dn_list.end());

    // construct poisson probability distribution for number of hadrons
    std::poisson_distribution<int> poisson_hadrons(dn_tot);


    // sample events for each freezeout cell
    for(long ievent = 0; ievent < Nevents; ievent++)
    {
      int N_hadrons = poisson_hadrons(generator_poisson);   // sample total number of hadrons in FO cell

      for(int n = 0; n < N_hadrons; n++)
      {
        int chosen_index = particle_type(generator_type);   // chosen index of sampled particle type

        double mass = Mass[chosen_index];                   // mass of sampled particle in GeV
        double mass_squared = mass * mass;

        double sign = Sign[chosen_index];                   // quantum statistics sign
        double baryon = Baryon[chosen_index];               // baryon number
        double chem = baryon * upsilonB;                    // chemical potential term

        int mcid = MCID[chosen_index];                      // mc_id


        // sample LRF momentum and transform

        LRF_Momentum pLRF = sample_momentum(generator_momentum, &acceptances, &samples, mass, sign, lambda, chem);
        pLRF = rescale_momentum_famod(pLRF, mass_squared, Bxx, Bxy, Bxz, Byy, Byz, Bzz);

        double E = pLRF.E;                                  // get pLRF components
        double px = pLRF.px;
        double py = pLRF.py;
        double pz = pLRF.pz;


        // compute flux weight and determine whether to keep sampled particle

        double p_dsigma = E * dst  -  px * dsx  -  py * dsy  -  pz * dsz;
        double w_flux = max(0., p_dsigma) / (E * ds_max);

        if(canonical(generator_momentum) < w_flux)                      // keep particle
        {
          Lab_Momentum pLab(pLRF);
          pLab.boost_pLRF_to_lab_frame(basis_vectors, ut, ux, uy, un);  // get the lab frame momentum

          Sampled_Particle new_particle;

          new_particle.chosen_index = chosen_index;                     // set sampled particle info
          new_particle.mcID = mcid;
          new_particle.tau = tau;
          new_particle.x = x;
          new_particle.y = y;
          new_particle.mass = mass;
          new_particle.px = pLab.px;
          new_particle.py = pLab.py;

          double E;                                                     // cartesian lab energy
          double pz;                                                    // cartesian lab longitudinal momentum
          double rapidity;                                              // lab momentum rapidity

          if(DIMENSION == 2)
          {
            rapidity = uniform_rapidity_distribution(generator_rapidity, y_max);

            double sinhy = sinh(rapidity);
            double coshy = sqrt(1.  +  sinhy * sinhy);

            double ptau = pLab.ptau;
            double tau_pn = tau * pLab.pn;
            double mT = sqrt(ptau * ptau  -  tau_pn * tau_pn);

            sinheta = (ptau * sinhy  -  tau_pn * coshy) / mT;
            eta = asinh(sinheta);
            cosheta = sqrt(1.  +  sinheta * sinheta);

            pz = mT * sinhy;
            E = mT * coshy;
          }
          else
          {
            pz = tau * pLab.pn * cosheta  +  pLab.ptau * sinheta;
            E = sqrt(mass_squared  +  pLab.px * pLab.px  +  pLab.py * pLab.py  +  pz * pz);
            rapidity = 0.5 * log((E + pz) / (E - pz));
          }

          new_particle.eta = eta;
          new_particle.t = tau * cosheta;
          new_particle.z = tau * sinheta;
          new_particle.E = E;
          new_particle.pz = pz;

          if(TEST_SAMPLER)
          {
            sample_dN_dy(chosen_index, rapidity);                       // bin the distributions (avoids memory bottleneck)
            sample_dN_deta(chosen_index, eta);
            sample_dN_2pipTdpTdy(chosen_index, rapidity, pLab.px, pLab.py);
            sample_dN_dphipdy(chosen_index, rapidity, pLab.px, pLab.py);
            sample_vn(chosen_index, rapidity, pLab.px, pLab.py);
            sample_dN_dX(chosen_index, tau, x, y);
          } else {
            //CAREFUL push_back is not a thread-safe operation
            //how should we modify for GPU version?
            #pragma omp critical
            particle_event_list[ievent].push_back(new_particle);        // append sampled particle to event list
          }
        } // keep sampled particle
      } // hadrons (n)
    } // events (ievent)
  } // freezeout cells (icell)

  printf("Momentum sampling efficiency = %f %%\n", (float)(100.0 * (double)acceptances / (double)samples));

  printf("pl went negative for %ld / %ld cells\n\n", plpt_negative, FO_length);
  printf("Number of reconstruction failures = %ld\n", reconstruction_fail);
}



