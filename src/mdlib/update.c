/*
 *       $Id$
 *
 *       This source code is part of
 *
 *        G   R   O   M   A   C   S
 *
 * GROningen MAchine for Chemical Simulations
 *
 *            VERSION 2.0
 * 
 * Copyright (c) 1991-1997
 * BIOSON Research Institute, Dept. of Biophysical Chemistry
 * University of Groningen, The Netherlands
 * 
 * Please refer to:
 * GROMACS: A message-passing parallel molecular dynamics implementation
 * H.J.C. Berendsen, D. van der Spoel and R. van Drunen
 * Comp. Phys. Comm. 91, 43-56 (1995)
 *
 * Also check out our WWW page:
 * http://rugmd0.chem.rug.nl/~gmx
 * or e-mail to:
 * gromacs@chem.rug.nl
 *
 * And Hey:
 * Gromacs Runs On Most of All Computer Systems
 */
static char *SRCID_update_c = "$Id$";

#include <stdio.h>
#include <math.h>

#include "assert.h"
#include "sysstuff.h"
#include "smalloc.h"
#include "typedefs.h"
#include "nrnb.h"
#include "led.h"
#include "physics.h"
#include "macros.h"
#include "vveclib.h"
#include "vec.h"
#include "main.h"
#include "confio.h"
#include "update.h"
#include "random.h"
#include "futil.h"
#include "mshift.h"
#include "tgroup.h"
#include "force.h"
#include "names.h"
#include "txtdump.h"
#include "mdrun.h"
#include "copyrite.h"
#include "constr.h"
#include "edsam.h"
#include "pull.h"

static void calc_g(rvec x_unc,rvec x_cons,rvec g,double mdt_2)
{
  int d;
  
  for(d=0; (d<DIM);d++) 
    g[d]=(x_cons[d]-x_unc[d])*mdt_2;
}

static void do_shake_corr(rvec xold,rvec x,rvec v,double dt_1)
{
  int    d;

  for(d=0; (d<DIM);d++) 
    v[d]=((double) x[d]-(double) xold[d])*dt_1;
}

static void do_both(rvec xold,rvec x_unc,rvec x,rvec g,
		    rvec v,real mdt_2,real dt_1)
{
  real xx,yy,zz;

  xx=x[XX];
  yy=x[YY];
  zz=x[ZZ];
  g[XX]=(xx-x_unc[XX])*mdt_2;
  g[YY]=(yy-x_unc[YY])*mdt_2;
  g[ZZ]=(zz-x_unc[ZZ])*mdt_2;
  v[XX]=(xx-xold [XX])*dt_1;
  v[YY]=(yy-xold [YY])*dt_1;
  v[ZZ]=(zz-xold [ZZ])*dt_1;
}

static void do_update(int start,int homenr,double dt,
		      rvec lamb[],t_grp_acc gstat[],
		      rvec accel[],rvec freezefac[],
		      real invmass[],ushort ptype[],
		      ushort cFREEZE[],ushort cACC[],ushort cTC[],
		      rvec x[],rvec xprime[],rvec v[],rvec vold[],rvec f[])
{
  double w_dt;
  int    gf,ga,gt;
  real   vn,vv,va,vb;
  real   uold,lg;
  int    n,d;
  
  for (n=start; (n<start+homenr); n++) {  
    w_dt = invmass[n]*dt;
    gf   = cFREEZE[n];
    ga   = cACC[n];
    gt   = cTC[n];
    
    for (d=0; (d<DIM); d++) {
      vn             = v[n][d];
      lg             = lamb[gt][d];
      vold[n][d]     = vn;
      
      if ((ptype[n] != eptDummy) && (ptype[n] != eptShell) && (freezefac[gf][d] != 0)) {
	vv             = lg*(vn + f[n][d]*w_dt);
	  
	/* do not scale the mean velocities u */
	uold           = gstat[ga].uold[d];
	va             = vv + accel[ga][d]*dt;
	vb             = va + (1.0-lg)*uold;
	v[n][d]        = vb;
	xprime[n][d]   = x[n][d]+vb*dt;
      }
      else
	xprime[n][d]   = x[n][d];
    }
  }
}

static void do_update_lang(int start,int homenr,double dt,
			   rvec freezefac[],ushort ptype[],ushort cFREEZE[],
			   rvec x[],rvec xprime[],rvec v[],rvec vold[],
			   rvec f[],real temp, real fr, int *seed)
{
  const unsigned long im = 0xffff;
  const unsigned long ia = 1093;
  const unsigned long ic = 18257;
  int    gf;
  real   vn,vv;
  real   rfac,invfr,rhalf,jr;
  int    n,d;
  ulong  jran;

  /* (r-0.5) n times:  var_n = n * var_1 = n/12
     n=4:  var_n = 1/3, so multiply with 3 */
  
  rfac  = sqrt(3.0 * 2.0*BOLTZ*temp/(fr*dt));
  rhalf = 2.0*rfac; 
  rfac  = rfac/(real)im;
  invfr = 1.0/fr;
  
  jran = (unsigned long)((real)im*rando(seed));

  for (n=start; (n<start+homenr); n++) {  
    gf   = cFREEZE[n];
    for (d=0; (d<DIM); d++) {
      vn             = v[n][d];
      vold[n][d]     = vn;
      if ((ptype[n]!=eptDummy) && (ptype[n]!=eptShell) && freezefac[gf][d]) {
	jran = (jran*ia+ic) & im;
	jr = (real)jran;
	jran = (jran*ia+ic) & im;
	jr += (real)jran;
	jran = (jran*ia+ic) & im;
	jr += (real)jran;
	jran = (jran*ia+ic) & im;
	jr += (real)jran;
	vv             = invfr*f[n][d] + rfac * jr - rhalf;
	v[n][d]        = vv;
	xprime[n][d]   = x[n][d]+v[n][d]*dt;
      } else
	xprime[n][d]   = x[n][d];
    }
  }
}

static void shake_calc_vir(FILE *log,int nxf,rvec x[],rvec f[],tensor vir,
                           t_commrec *cr)
{
  int    i,m,n;
  matrix dvir;
  
  clear_mat(dvir);
  for(i=0; (i<nxf); i++) {
    for(m=0; (m<DIM); m++)
      for(n=0; (n<DIM); n++)
        dvir[m][n]+=x[i][m]*f[i][n];
  }
  
  for(m=0; (m<DIM); m++)
    for(n=0; (n<DIM); n++)
      vir[m][n]-=0.5*dvir[m][n];
}

static void dump_it_all(FILE *fp,char *title,
		 int natoms,rvec x[],rvec xp[],rvec v[],
		 rvec vold[],rvec f[])
{
#ifdef DEBUG
  fprintf(fp,"%s\n",title);
  pr_rvecs(fp,0,"x",x,natoms);
  pr_rvecs(fp,0,"xp",xp,natoms);
  pr_rvecs(fp,0,"v",v,natoms);
  pr_rvecs(fp,0,"vold",vold,natoms);
  pr_rvecs(fp,0,"f",f,natoms);
#endif
}

void calc_ke_part(bool bFirstStep,int start,int homenr,
		  rvec vold[],rvec v[],rvec vt[],
		  t_grpopts *opts,t_mdatoms *md,t_groups *grps,
		  t_nrnb *nrnb,real lambda,real *dvdlambda)
{
  int          g,d,n,ga,gt;
  rvec         v_corrt;
  real         hm,vvt,vct;
  t_grp_tcstat *tcstat=grps->tcstat;
  t_grp_acc    *grpstat=grps->grpstat;
  real         dvdl;

  /* group velocities are calculated in update_grps and
   * accumulated in acumulate_groups.
   * Now the partial global and groups ekin.
   */
  for (g=0; (g<opts->ngtc); g++)
    clear_mat(grps->tcstat[g].ekin); 
    
  if (bFirstStep) {
    for(n=start; (n<start+homenr); n++) {
      copy_rvec(v[n],vold[n]);
    }
    for (g=0; (g<opts->ngacc); g++) {
      for(d=0; (d<DIM); d++)
	grps->grpstat[g].ut[d]=grps->grpstat[g].u[d];
    }
  }
  else {
    for (g=0; (g<opts->ngacc); g++) { 
      for(d=0; (d<DIM); d++)
	grps->grpstat[g].ut[d]=0.5*(grps->grpstat[g].u[d]+
				    grps->grpstat[g].uold[d]);
    }
  }

  dvdl = 0;
  for(n=start; (n<start+homenr); n++) {  
    ga   = md->cACC[n];
    gt   = md->cTC[n];
    hm   = 0.5*md->massT[n];
    
    for(d=0; (d<DIM); d++) {
      vvt        = 0.5*(v[n][d]+vold[n][d]);
      vt[n][d]   = vvt;
      vct        = vvt - grpstat[ga].ut[d];
      v_corrt[d] = vct;
    }
    for(d=0; (d<DIM); d++) {
      tcstat[gt].ekin[XX][d]+=hm*v_corrt[XX]*v_corrt[d];
      tcstat[gt].ekin[YY][d]+=hm*v_corrt[YY]*v_corrt[d];
      tcstat[gt].ekin[ZZ][d]+=hm*v_corrt[ZZ]*v_corrt[d];
    }
    if (md->bPerturbed[n]) {
      dvdl-=0.5*(md->massB[n]-md->massA[n])*iprod(v_corrt,v_corrt);
    }
  }
  *dvdlambda += dvdl;
  
#ifdef DEBUG
  fprintf(stdlog,"ekin: U=(%12e,%12e,%12e)\n",
	  grpstat[0].ut[XX],grpstat[0].ut[YY],grpstat[0].ut[ZZ]);
  fprintf(stdlog,"ekin: %12e\n",trace(tcstat[0].ekin));
#endif

  inc_nrnb(nrnb,eNR_EKIN,homenr);
}

void update(int          natoms, 	/* number of atoms in simulation */
	    int      	 start,
	    int          homenr,	/* number of home particles 	*/
	    int          step,
	    real         lambda,
	    real         *dvdlambda, /* FEP stuff */
	    t_inputrec   *ir,           /* input record with constants 	*/
	    bool         bFirstStep,   
	    t_mdatoms    *md,
	    rvec         x[],	/* coordinates of home particles */
	    t_graph      *graph,
	    rvec         shift_vec[],	
	    rvec         force[], 	/* forces on home particles 	*/
	    rvec         delta_f[],
	    rvec         vold[],	/* Old velocities		   */
	    rvec         v[], 		/* velocities of home particles */
	    rvec         vt[],  	/* velocity at time t 		*/
	    tensor       pressure, 	/* instantaneous pressure tensor */
	    tensor       box,  		/* instantaneous box lengths 	*/
	    t_topology   *top,
	    t_groups     *grps,
	    tensor       vir_part,
	    t_commrec    *cr,
	    t_nrnb       *nrnb,
	    bool         bTYZ,
	    bool         bDoUpdate,
	    t_edsamyn    *edyn,
	    t_pull       *pulldata)
{
  static char      buf[256];
  static bool      bFirst=TRUE;
  static rvec      *xprime,*x_unc=NULL;
  static int       ngtc,ngacc,ngfrz;
  static rvec      *lamb,*freezefac;
  static t_edpar   edpar;
  static bool      bConstraints;

  t_idef           *idef=&(top->idef);
  double           dt;
  real             dt_1,dt_2;
  int              i,n,m,g;

  set_led(UPDATE_LED);

  if (bFirst) {
    bConstraints = init_constraints(stdlog,top,ir,md,
				    start,homenr);
    bConstraints = bConstraints || pulldata->bPull;

    if (edyn->bEdsam) 
      init_edsam(stdlog,top,md,start,homenr,x,box,
		 edyn,&edpar);
    
    /* Allocate memory for xold, original atomic positions
     * and for xprime.
     */
    snew(xprime,natoms);
    snew(x_unc,homenr);

    /* Freeze Factor: If a dimension of a group has to be frozen,
     * the corresponding freeze fac will be 0.0 otherwise 1.0
     * This is implemented by multiplying the CHANGE in position
     * by freeze fac (also in do_pcoupl)
     *
     * Coordinates in shake can be frozen by setting the invmass
     * of a particle to 0.0 (===> Infinite mass!)
     */
    ngfrz=ir->opts.ngfrz;
    snew(freezefac,ngfrz);
    for(n=0; (n<ngfrz); n++)
      for(m=0; (m<DIM); m++) {
	freezefac[n][m]=(ir->opts.nFreeze[n][m]==0) ? 1.0 : 0.0;
/* 	printf("n %d m %d ff %g\n",n,m,freezefac[n][m]); */
      }
/*     for(i=0; (i<natoms); i++) */
/*       printf("%d fg %d\n",i,md->cFREEZE[i]); */
    /* Copy the pointer to the external acceleration in the opts */
    ngacc=ir->opts.ngacc;
    
    ngtc=ir->opts.ngtc;
    snew(lamb,ir->opts.ngtc);
    
    /* done with initializing */
    bFirst=FALSE;
  }
  
  dt   = ir->delta_t;
  dt_1 = 1.0/dt;
  dt_2 = 1.0/(dt*dt);
  
  for(i=0; (i<ngtc); i++) {
    real l=grps->tcstat[i].lambda;
    
    if (bTYZ)
      lamb[i][XX]=1;
    else
      lamb[i][XX]=l;
    lamb[i][YY]=l;
    lamb[i][ZZ]=l;
  }

  if (bDoUpdate) {  
    /* update mean velocities */
    for (g=0; (g<ngacc); g++) {
      copy_rvec(grps->grpstat[g].u,grps->grpstat[g].uold);
      clear_rvec(grps->grpstat[g].u);
    }
    
    /* Now do the actual update of velocities and positions */
    where();
    dump_it_all(stdlog,"Before update",natoms,x,xprime,v,vold,force);
    if (ir->eI==eiMD)
      /* use normal version of update */
      do_update(start,homenr,dt,
		lamb,grps->grpstat,
		ir->opts.acc,freezefac,
		md->invmass,md->ptype,
		md->cFREEZE,md->cACC,md->cTC,
		x,xprime,v,vold,force);
    else if (ir->eI==eiLD) 
      do_update_lang(start,homenr,dt,
		     freezefac,md->ptype,md->cFREEZE,
		     x,xprime,v,vold,force,
		     ir->ld_temp,ir->ld_fric,&ir->ld_seed);
    else
      fatal_error(0,"Don't know how to update coordinates");

    where();
    inc_nrnb(nrnb,eNR_UPDATE,homenr);
    dump_it_all(stdlog,"After update",natoms,x,xprime,v,vold,force);
  }
  else {
    /* If we're not updating we're doing shakefirst!
     * In this case the extra coordinates are passed in v array
     */
    for(n=start; (n<start+homenr); n++) {
      copy_rvec(v[n],xprime[n]);
    }
  }

  /* 
   *  Steps (7C, 8C)
   *  APPLY CONSTRAINTS:
   *  BLOCK SHAKE 
   */
 
  if (bConstraints) {
    /* Copy Unconstrained X to temp array */
    for(n=start; (n<start+homenr); n++)
      copy_rvec(xprime[n],x_unc[n-start]);

    /* Constrain the coordinates xprime */
    constrain(stdlog,top,ir,step,md,start,homenr,x,xprime,box,
	      lambda,dvdlambda,nrnb);

    where();

    dump_it_all(stdlog,"After Shake",natoms,x,xprime,v,vold,force);

    /* apply Essential Dynamics constraints when required */
    if (edyn->bEdsam)
      do_edsam(stdlog,top,ir,step,md,start,homenr,xprime,x,
	       x_unc,force,box,edyn,&edpar,bDoUpdate);

    /* apply pull constraints when required. Act on xprime, the SHAKED
       coordinates.  Don't do anything to f */
    if (pulldata->bPull && pulldata->runtype != eAfm && 
	pulldata->runtype != eUmbrella &&
	pulldata->runtype != eTest) 
      pull(pulldata,xprime,force,box,top,dt,step,homenr,md); 
    
    where();

    if (bDoUpdate) {
      real mdt_2;
      
      for(n=start; (n<start+homenr); n++) {
	mdt_2 = dt_2*md->massT[n];
	do_both(x[n],x_unc[n-start],xprime[n],delta_f[n],
		v[n],mdt_2,dt_1);
      }
      where();

      inc_nrnb(nrnb,eNR_SHAKE_V,homenr);
      dump_it_all(stdlog,"After Shake-V",natoms,x,xprime,v,vold,force);
      where();
      
      /* Calculate virial due to shake (for this proc) */
      calc_vir(stdlog,homenr,&(x[start]),&(delta_f[start]),vir_part,cr);
      inc_nrnb(nrnb,eNR_SHAKE_VIR,homenr);
      where();
    }
  }

  
  /* We must always unshift here, also if we did not shake
   * x was shifted in do_force */
  where();
  if ((graph->nnodes > 0) && bDoUpdate && (ir->eBox != ebtNONE)) {
    unshift_x(graph,shift_vec,x,xprime);
    inc_nrnb(nrnb,eNR_SHIFTX,graph->nnodes);
    for(n=start; (n<graph->start); n++)
      copy_rvec(xprime[n],x[n]);
    for(n=graph->start+graph->nnodes; (n<start+homenr); n++)
      copy_rvec(xprime[n],x[n]);
  }
  else {
    for(n=start; (n<start+homenr); n++)
      copy_rvec(xprime[n],x[n]);
  }
  dump_it_all(stdlog,"After unshift",natoms,x,xprime,v,vold,force);
  where();
  
  if (bDoUpdate) {  
    update_grps(start,homenr,grps,&(ir->opts),v,md);
    do_pcoupl(ir,step,pressure,box,start,homenr,x,md->cFREEZE,nrnb,freezefac);
    where();
  }

  clr_led(UPDATE_LED);
}
