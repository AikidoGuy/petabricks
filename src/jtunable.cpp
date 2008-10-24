/***************************************************************************
 *   Copyright (C) 2008 by Jason Ansel                                     *
 *   jansel@csail.mit.edu                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "jtunable.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

jalib::JTunableConfiguration jalib::JTunableManager::getCurrentConfiguration() const{
  JTunableConfiguration c;
  for(const_iterator i=begin(); i!=end(); ++i)
    c[*i] = *(*i);
  return c;
}



#ifdef HAVE_LIBGSL

#include <math.h>
#include <gsl/gsl_siman.h>

namespace { //file local

/* set up parameters for this simulated annealing run */
/* how many points do we try before stepping */
#define N_TRIES 200
/* how many iterations for each T? */
#define ITERS_FIXED_T 1000
/* max step size in random walk */
#define STEP_SIZE 1.0
/* Boltzmann constant */
#define K 1.0
/* initial temperature */
#define T_INITIAL 0.008
/* damping factor for temperature */
#define MU_T 1.003 
#define T_MIN 2.0e-6

gsl_siman_params_t params = {N_TRIES, ITERS_FIXED_T, STEP_SIZE, K, T_INITIAL, MU_T, T_MIN};

// return the energy of a configuration xp.
double testEnergy(void *xp)
{
  double x = * ((double *) xp);
  return exp(-pow((x-1.0),2.0))*sin(8*x);
}

// modify the configuration xp using a random step taken from the generator r, up to a maximum distance of step_size.
void step(const gsl_rng * r, void *xp, double step_size)
{
  double old_x = *((double *) xp);
  double new_x;

  double u = gsl_rng_uniform(r);
  new_x = u * 2 * step_size - step_size + old_x;

  memcpy(xp, &new_x, sizeof(new_x));
}

// return the distance between two configurations xp and yp.
double getDistance(void *xp, void *yp)
{
  double x = *((double *) xp);
  double y = *((double *) yp);
  return fabs(x - y);
}

// print the contents of the configuration xp.
void printCfg(void *xp)
{
  printf ("%12g", *((double *) xp));
}

void cfgCopy(void *source, void *dest){}
void* cfgCreate(void *xp){}
void cfgDestroy(void *xp){}

int foo(int argc, char *argv[]){
  const gsl_rng_type * T;
  gsl_rng * r;

  double x_initial = 15.5;

  gsl_rng_env_setup();

  T = gsl_rng_default;
  r = gsl_rng_alloc(T);

  gsl_siman_solve(r, &x_initial, testEnergy, step, getDistance, printCfg,
                  cfgCopy, cfgCreate, cfgDestroy,
                  0, params);

  gsl_rng_free (r);
  return 0;
}

}
#endif//HAVE_LIBGSL
