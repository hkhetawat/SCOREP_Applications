/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing author: Trung Dac Nguyen (ORNL)
------------------------------------------------------------------------- */

#include "pair_yukawa_colloid_gpu.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "atom.h"
#include "atom_vec.h"
#include "comm.h"
#include "force.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "integrate.h"
#include "memory.h"
#include "error.h"
#include "neigh_request.h"
#include "universe.h"
#include "update.h"
#include "domain.h"
#include "gpu_extra.h"
#include "suffix.h"

using namespace LAMMPS_NS;

// External functions from cuda library for atom decomposition

int ykcolloid_gpu_init(const int ntypes, double **cutsq, double **host_a,
                 double **host_offset, double *special_lj, const int inum,
                 const int nall, const int max_nbors,  const int maxspecial,
                 const double cell_size, int &gpu_mode, FILE *screen,
                 const double kappa);
void ykcolloid_gpu_clear();
int ** ykcolloid_gpu_compute_n(const int ago, const int inum_full,
                        const int nall, double **host_x, int *host_type,
                        double *sublo, double *subhi, tagint *tag, int **nspecial,
                        tagint **special, const bool eflag, const bool vflag,
                        const bool eatom, const bool vatom, int &host_start,
                        int **ilist, int **jnum, const double cpu_time,
                        bool &success, double *host_rad);
void ykcolloid_gpu_compute(const int ago, const int inum_full,
                     const int nall, double **host_x, int *host_type,
                     int *ilist, int *numj, int **firstneigh,
                     const bool eflag, const bool vflag,
                     const bool eatom, const bool vatom, int &host_start,
                     const double cpu_time, bool &success, double *host_rad);
double ykcolloid_gpu_bytes();

/* ---------------------------------------------------------------------- */

PairYukawaColloidGPU::PairYukawaColloidGPU(LAMMPS *lmp) : PairYukawaColloid(lmp),
  gpu_mode(GPU_FORCE)
{
  respa_enable = 0;
  reinitflag = 0;
  cpu_time = 0.0;
  suffix_flag |= Suffix::GPU;
  GPU_EXTRA::gpu_ready(lmp->modify, lmp->error);
}

/* ----------------------------------------------------------------------
   free all arrays
------------------------------------------------------------------------- */

PairYukawaColloidGPU::~PairYukawaColloidGPU()
{
  ykcolloid_gpu_clear();
}

/* ---------------------------------------------------------------------- */

void PairYukawaColloidGPU::compute(int eflag, int vflag)
{
  ev_init(eflag,vflag);

  int nall = atom->nlocal + atom->nghost;
  int inum, host_start;

  bool success = true;
  int *ilist, *numneigh, **firstneigh;
  if (gpu_mode != GPU_FORCE) {
    inum = atom->nlocal;
    firstneigh = ykcolloid_gpu_compute_n(neighbor->ago, inum, nall,
                                         atom->x, atom->type,
                                         domain->sublo,
                                         domain->subhi, atom->tag,
                                         atom->nspecial, atom->special,
                                         eflag, vflag, eflag_atom,
                                         vflag_atom, host_start, &ilist,
                                         &numneigh, cpu_time,
                                         success, atom->radius);
  } else {
    inum = list->inum;
    ilist = list->ilist;
    numneigh = list->numneigh;
    firstneigh = list->firstneigh;
    ykcolloid_gpu_compute(neighbor->ago, inum, nall, atom->x, atom->type,
                          ilist, numneigh, firstneigh, eflag, vflag,
                          eflag_atom, vflag_atom, host_start, cpu_time,
                          success, atom->radius);
  }
  if (!success)
    error->one(FLERR,"Insufficient memory on accelerator");

  if (host_start<inum) {
    cpu_time = MPI_Wtime();
    cpu_compute(host_start, inum, eflag, vflag, ilist, numneigh, firstneigh);
    cpu_time = MPI_Wtime() - cpu_time;
  }
}

/* ----------------------------------------------------------------------
   init specific to this pair style
------------------------------------------------------------------------- */

void PairYukawaColloidGPU::init_style()
{
  if (!atom->sphere_flag)
    error->all(FLERR,"Pair yukawa/colloid/gpu requires atom style sphere");

  if (force->newton_pair)
    error->all(FLERR,"Cannot use newton pair with yukawa/colloid/gpu pair style");

  // Repeat cutsq calculation because done after call to init_style
  double maxcut = -1.0;
  double cut;
  for (int i = 1; i <= atom->ntypes; i++) {
    for (int j = i; j <= atom->ntypes; j++) {
      if (setflag[i][j] != 0 || (setflag[i][i] != 0 && setflag[j][j] != 0)) {
        cut = init_one(i,j);
        cut *= cut;
        if (cut > maxcut)
          maxcut = cut;
        cutsq[i][j] = cutsq[j][i] = cut;
      } else
        cutsq[i][j] = cutsq[j][i] = 0.0;
    }
  }
  double cell_size = sqrt(maxcut) + neighbor->skin;

  int maxspecial=0;
  if (atom->molecular)
    maxspecial=atom->maxspecial;
  int success = ykcolloid_gpu_init(atom->ntypes+1, cutsq, a,
                                   offset, force->special_lj, atom->nlocal,
                                   atom->nlocal+atom->nghost, 300, maxspecial,
                                   cell_size, gpu_mode, screen, kappa);
  GPU_EXTRA::check_flag(success,error,world);

  if (gpu_mode == GPU_FORCE) {
    int irequest = neighbor->request(this,instance_me);
    neighbor->requests[irequest]->half = 0;
    neighbor->requests[irequest]->full = 1;
  }
}

/* ---------------------------------------------------------------------- */

double PairYukawaColloidGPU::memory_usage()
{
  double bytes = Pair::memory_usage();
  return bytes + ykcolloid_gpu_bytes();
}

/* ---------------------------------------------------------------------- */

void PairYukawaColloidGPU::cpu_compute(int start, int inum, int eflag,
                                       int /* vflag */, int *ilist,
                                       int *numneigh, int **firstneigh) {
  int i,j,ii,jj,jnum,itype,jtype;
  double xtmp,ytmp,ztmp,delx,dely,delz,evdwl,fpair,radi,radj;
  double r,rsq,rinv,screening,forceyukawa,factor;
  int *jlist;

  double **x = atom->x;
  double **f = atom->f;
  int *type = atom->type;
  double *radius = atom->radius;
  double *special_lj = force->special_lj;

  // loop over neighbors of my atoms

  for (ii = start; ii < inum; ii++) {
    i = ilist[ii];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    itype = type[i];
    radi = radius[i];
    jlist = firstneigh[i];
    jnum = numneigh[i];

    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      factor = special_lj[sbmask(j)];
      j &= NEIGHMASK;

      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rsq = delx*delx + dely*dely + delz*delz;
      jtype = type[j];
      radj = radius[j];

      if (rsq < cutsq[itype][jtype]) {
        r = sqrt(rsq);
        rinv = 1.0/r;
        screening = exp(-kappa*(r-(radi+radj)));
        forceyukawa = a[itype][jtype] * screening;

        fpair = factor*forceyukawa * rinv;

        f[i][0] += delx*fpair;
        f[i][1] += dely*fpair;
        f[i][2] += delz*fpair;

        if (eflag) {
          evdwl = a[itype][jtype]/kappa * screening - offset[itype][jtype];
          evdwl *= factor;
        }

        if (evflag) ev_tally_full(i,evdwl,0.0,fpair,delx,dely,delz);
      }
    }
  }
}