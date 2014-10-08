#include "lulesh-hpx.h"

int get_bs_index(int i, int bi, int bs) {
  int r = hpx_get_num_ranks();
  //  printf("(%d, %d) -> %d\n", bi, i, (bi % r) + (bi/r)*bs*r + i*r);

  return (bi%r) + (bi/r)*bs*r + i*r;
}

void
SBN1(Domain *domain, hpx_netfuture_t sbn1)
{
  const int    nsTF = domain->sendTF[0];
  const int *sendTF = &domain->sendTF[1]; 

  int i,destLocalIdx,srcRemoteIdx,srcLocalIdx,distance;
  int           nx = domain->sizeX + 1;
  int           ny = domain->sizeY + 1;
  int           nz = domain->sizeZ + 1;
  for (i = 0; i < nsTF; ++i) {
    destLocalIdx = sendTF[i];
    srcRemoteIdx = destLocalIdx;
    srcLocalIdx = 25 - srcRemoteIdx;
    distance = -OFFSET[srcLocalIdx];
    const hpx_addr_t data_addr = hpx_gas_alloc(BUFSZ[destLocalIdx]);
    double *data;
    const bool pin_success = hpx_gas_try_pin(data_addr, (void**) &data);
    assert(pin_success);
    send_t      pack = SENDER[destLocalIdx];
    pack(nx, ny, nz, domain->nodalMass, data);
    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_lco_netfuture_setat(sbn1, get_bs_index((srcLocalIdx + (domain->rank+distance)*26)%26, domain->rank+distance, 26), BUFSZ[destLocalIdx], data_addr, lsync, HPX_NULL);
    hpx_lco_wait(lsync);
    hpx_lco_delete(lsync, HPX_NULL);
    hpx_gas_unpin(data_addr);
  }

  // wait for incoming data
  int nrTF = domain->recvTF[0];
  int *recvTF = &domain->recvTF[1];

  for (i = 0; i < nrTF; i++) {
    int srcLocalIdx = recvTF[i];
    int srcRemoteIdx = 25 - srcLocalIdx;
    const hpx_addr_t src_addr = hpx_lco_netfuture_getat(sbn1, get_bs_index((srcLocalIdx + domain->rank*26)%26, domain->rank, 26), BUFSZ[srcRemoteIdx]);
    double *src;
    const bool pin_success = hpx_gas_try_pin(src_addr, (void**) &src);
    assert(pin_success);
    recv_t unpack = RECEIVER[srcLocalIdx];
    unpack(nx, ny, nz, src, domain->nodalMass, 0);
    hpx_gas_unpin(src_addr);
  }
}

void
SBN3(hpx_netfuture_t sbn3_a,hpx_netfuture_t sbn3_b,Domain *domain, int rank)
{
  int nx = domain->sizeX + 1;
  int ny = domain->sizeY + 1;
  int nz = domain->sizeZ + 1;
  int tag = domain->cycle;
  int i,destLocalIdx;

  // pack outgoing data
  int nsTF = domain->sendTF[0];
  int *sendTF = &domain->sendTF[1];
  
  int gen = tag%2;
  for (i = 0; i < nsTF; i++) {
    int destLocalIdx = sendTF[i];
    // the neighbor this is being sent to
    int srcRemoteIdx = destLocalIdx;
    int srcLocalIdx = 25 - srcRemoteIdx;
    int distance = -OFFSET[srcLocalIdx];
    int sendcnt = XFERCNT[destLocalIdx];
    const hpx_addr_t data_addr = hpx_gas_alloc(BUFSZ[destLocalIdx]);
    double *data;
    const bool pin_success = hpx_gas_try_pin(data_addr, (void**) &data);
    send_t pack = SENDER[destLocalIdx];

    pack(nx, ny, nz, domain->fx, data);
    pack(nx, ny, nz, domain->fy, data + sendcnt);
    pack(nx, ny, nz, domain->fz, data + sendcnt*2);

    hpx_gas_unpin(data_addr);

    hpx_addr_t lsync = hpx_lco_future_new(0);
    if ( gen == 0 ) {
      hpx_lco_netfuture_setat(sbn3_a, get_bs_index((srcLocalIdx + 26*(domain->rank+distance))%26, domain->rank+distance, 26), BUFSZ[destLocalIdx], data_addr, lsync, HPX_NULL);
    } else {
      hpx_lco_netfuture_setat(sbn3_b, get_bs_index((srcLocalIdx + 26*(domain->rank+distance))%26, domain->rank+distance, 26), BUFSZ[destLocalIdx], data_addr, lsync, HPX_NULL);
    }
    hpx_lco_wait(lsync);
    hpx_lco_delete(lsync, HPX_NULL);
    
  } 

  // wait for incoming data
  int nrTF = domain->recvTF[0];
  int *recvTF = &domain->recvTF[1];

  hpx_addr_t src_addr;
  for (i = 0; i < nrTF; i++) {
    int srcLocalIdx = recvTF[i];
    int srcRemoteIdx = 25 - srcLocalIdx;
    if ( gen == 0 ) {
      src_addr = hpx_lco_netfuture_getat(sbn3_a, get_bs_index((srcLocalIdx + 26*(domain->rank + gen*2))%26, domain->rank, 26), BUFSZ[srcRemoteIdx]);
    } else {
      src_addr = hpx_lco_netfuture_getat(sbn3_b, get_bs_index((srcLocalIdx + 26*(domain->rank + gen*2))%26, domain->rank, 26), BUFSZ[srcRemoteIdx]);
    }
    double *src;
    const bool hpx_pin_success = hpx_gas_try_pin(src_addr, (void**) &src);
    assert(hpx_pin_success);
    int recvcnt = XFERCNT[srcLocalIdx];
    recv_t unpack = RECEIVER[srcLocalIdx];

    unpack(nx, ny, nz, src, domain->fx, 0);
    unpack(nx, ny, nz, src + recvcnt, domain->fy, 0);
    unpack(nx, ny, nz, src + recvcnt*2, domain->fz, 0);

    // unpin the buffer
    hpx_gas_unpin(src_addr);
  }

}

void
PosVel(hpx_netfuture_t posvel_a,hpx_netfuture_t posvel_b,Domain *domain, int rank)
{
  int nx = domain->sizeX + 1;
  int ny = domain->sizeY + 1;
  int nz = domain->sizeZ + 1;
  int tag = domain->cycle;
  int i,destLocalIdx;

  // pack outgoing data
  int nsFF = domain->sendFF[0];
  int *sendFF = &domain->sendFF[1];
  
  int gen = tag%2;
  for (i = 0; i < nsFF; i++) {
    int destLocalIdx = sendFF[i];
    // the neighbor this is being sent to
    int srcRemoteIdx = destLocalIdx;
    int srcLocalIdx = 25 - srcRemoteIdx;
    int distance = -OFFSET[srcLocalIdx];
    int sendcnt = XFERCNT[destLocalIdx];
    const hpx_addr_t data_addr = hpx_gas_alloc(BUFSZ[destLocalIdx]);
    double *data;
    const bool pin_success = hpx_gas_try_pin(data_addr, (void**) &data);
    send_t pack = SENDER[destLocalIdx];

    pack(nx, ny, nz, domain->x, data);
    pack(nx, ny, nz, domain->y, data + sendcnt);
    pack(nx, ny, nz, domain->z, data + sendcnt*2);
    pack(nx, ny, nz, domain->xd, data + sendcnt*3);
    pack(nx, ny, nz, domain->yd, data + sendcnt*4);
    pack(nx, ny, nz, domain->zd, data + sendcnt*5);

    hpx_gas_unpin(data_addr);

    hpx_addr_t lsync = hpx_lco_future_new(0);
    if ( gen == 0 ) {
      hpx_lco_netfuture_setat(posvel_a, get_bs_index((srcLocalIdx + 26*(domain->rank+distance))%26, domain->rank+distance, 26), BUFSZ[destLocalIdx], data_addr, lsync, HPX_NULL);
    } else {
      hpx_lco_netfuture_setat(posvel_b, get_bs_index((srcLocalIdx + 26*(domain->rank+distance))%26, domain->rank+distance, 26), BUFSZ[destLocalIdx], data_addr, lsync, HPX_NULL);
    }
    hpx_lco_wait(lsync);
    hpx_lco_delete(lsync, HPX_NULL);
  } 

  // wait for incoming data
  int nrFF = domain->recvFF[0];
  int *recvFF = &domain->recvFF[1];

  hpx_addr_t src_addr;
  for (i = 0; i < nrFF; i++) {
    int srcLocalIdx = recvFF[i];
    int srcRemoteIdx = 25 - srcLocalIdx;
    if ( gen == 0 ) {
      src_addr = hpx_lco_netfuture_getat(posvel_a, get_bs_index((srcLocalIdx + 26*(domain->rank))%26, domain->rank, 26), BUFSZ[srcRemoteIdx]);
    } else {
      src_addr = hpx_lco_netfuture_getat(posvel_b, get_bs_index((srcLocalIdx + 26*(domain->rank))%26, domain->rank, 26), BUFSZ[srcRemoteIdx]);
    }
    double *src;
    const bool hpx_pin_success = hpx_gas_try_pin(src_addr, (void**) &src);
    assert(hpx_pin_success);
    int recvcnt = XFERCNT[srcLocalIdx];
    recv_t unpack = RECEIVER[srcLocalIdx];

   unpack(nx, ny, nz, src, domain->x, 1);
   unpack(nx, ny, nz, src + recvcnt, domain->y, 1);
   unpack(nx, ny, nz, src + recvcnt*2, domain->z, 1);
   unpack(nx, ny, nz, src + recvcnt*3, domain->xd, 1);
   unpack(nx, ny, nz, src + recvcnt*4, domain->yd, 1);
   unpack(nx, ny, nz, src + recvcnt*5, domain->zd, 1);

    // unpin the buffer
    hpx_gas_unpin(src_addr);
  }

}

void
MonoQ(hpx_netfuture_t monoq_a,hpx_netfuture_t monoq_b,Domain *domain)
{
  int nx = domain->sizeX;
  int ny = domain->sizeY;
  int nz = domain->sizeZ;
  int tag = domain->cycle;
  int i,destLocalIdx;

  double *delv_xi = domain->delv_xi;
  double *delv_eta = domain->delv_eta;
  double *delv_zeta = domain->delv_zeta;

  int numElem = domain->numElem;
  int planeElem = nx*nx;

  // pack outgoing data
  int nsTT = domain->sendTT[0];
  int *sendTT = &domain->sendTT[1];

  int gen = tag%2;
  for (i = 0; i < nsTT; i++) {
    int destLocalIdx = sendTT[i];

    // the neighbor this is being sent to
    int srcRemoteIdx = destLocalIdx;
    int srcLocalIdx = 25 - srcRemoteIdx;
    int distance = -OFFSET[srcLocalIdx];
    const hpx_addr_t data_addr = hpx_gas_alloc(BUFSZ[destLocalIdx]);
    double *data;
    const bool pin_success = hpx_gas_try_pin(data_addr, (void**) &data);
    send_t pack = SENDER[destLocalIdx];

    pack(nx, ny, nz, delv_xi, data);
    pack(nx, ny, nz, delv_eta, data + planeElem);
    pack(nx, ny, nz, delv_zeta, data + planeElem*2);

    hpx_gas_unpin(data_addr);

    hpx_addr_t lsync = hpx_lco_future_new(0);
    if ( gen == 0 ) {
      hpx_lco_netfuture_setat(monoq_a, get_bs_index((srcLocalIdx + 26*(domain->rank+distance))%26, domain->rank+distance, 26), BUFSZ[destLocalIdx], data_addr, lsync, HPX_NULL);
    } else {
      hpx_lco_netfuture_setat(monoq_b, get_bs_index((srcLocalIdx + 26*(domain->rank+distance))%26, domain->rank+distance, 26), BUFSZ[destLocalIdx], data_addr, lsync, HPX_NULL);
    }
    hpx_lco_wait(lsync);
    hpx_lco_delete(lsync, HPX_NULL);
  } 

  // wait for incoming data
  int nrTT = domain->recvTT[0];
  int *recvTT = &domain->recvTT[1];

  // move pointers to the ghost area
  delv_xi += numElem;
  delv_eta += numElem;
  delv_zeta += numElem;

  hpx_addr_t src_addr;
  for (i = 0; i < nrTT; i++) {
    int srcLocalIdx = recvTT[i];
    int srcRemoteIdx = 25 - srcLocalIdx;
    if ( gen == 0 ) {
      src_addr = hpx_lco_netfuture_getat(monoq_a, get_bs_index((srcLocalIdx + 26*(domain->rank))%26, domain->rank, 26), BUFSZ[srcRemoteIdx]);
    } else {
      src_addr = hpx_lco_netfuture_getat(monoq_b, get_bs_index((srcLocalIdx + 26*(domain->rank))%26, domain->rank, 26), BUFSZ[srcRemoteIdx]);
    }
    double *src;
    const bool hpx_pin_success = hpx_gas_try_pin(src_addr, (void**) &src);
    assert(hpx_pin_success);

    memcpy(delv_xi + i*planeElem, src, sizeof(double)*planeElem);
    memcpy(delv_eta + i*planeElem, src + planeElem, sizeof(double)*planeElem);
    memcpy(delv_zeta + i*planeElem, src + planeElem*2, sizeof(double)*planeElem);

    // unpin the buffer
    hpx_gas_unpin(src_addr);
  }
  
}

void send1(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[0];
}

void send2(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  for (i = 0; i < nx; i++)
    dest[i] = src[i];
}

void send3(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx - 1];
}

void send4(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  for (i = 0; i < ny; i++)
    dest[i] = src[i*nx];
}

void send5(int nx, int ny, int nz, double *src, double *dest)
{
  memcpy(dest, src, nx*ny*sizeof(double));
}

void send6(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx - 1];
  for (i = 0; i < ny; i++)
    dest[i] = offsrc[i*nx];
}

void send7(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*(ny - 1)];
}

void send8(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*(ny - 1)];
  for (i = 0; i < nx; i++)
    dest[i] = offsrc[i];
}

void send9(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny - 1];
}

void send10(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  for (i = 0; i < nz; i++)
    dest[i] = src[i*nx*ny];
}

void send11(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  for (i = 0; i < nz; i++)
    memcpy(&dest[i*nx], &src[i*nx*ny], nx*sizeof(double));
}

void send12(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx - 1];
  for (i = 0; i < nz; i++)
    dest[i] = offsrc[i*nx*ny];
}

void send13(int nx, int ny, int nz, double *src, double *dest)
{
  int i, j;
  for (i = 0; i < nz; i++) {
    double *offsrc = &src[i*nx*ny];
    for (j = 0; j < ny; j++)
      dest[i*ny + j] = offsrc[j*nx];
  }
}

void send14(int nx, int ny, int nz, double *src, double *dest)
{
  int i, j;
  for (i = 0; i < nz; i++) {
    double *offsrc = &src[i*nx*ny + nx - 1];
    for (j = 0; j < ny; j++)
      dest[i*ny + j] = offsrc[j*nx];
  }
}

void send15(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*(ny - 1)];
  for (i = 0; i < nz; i++)
    dest[i] = offsrc[i*nx*ny];
}

void send16(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*(ny - 1)];
  for (i = 0; i < nz; i++)
    memcpy(&dest[i*nx], &offsrc[i*nx*ny], nx*sizeof(double));
}

void send17(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*ny - 1];
  for (i = 0; i < nz; i++)
    dest[i] = offsrc[i*nx*ny];
}

void send18(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny*(nz - 1)];
}

void send19(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*ny*(nz - 1)];
  for (i = 0; i < nx; i++)
    dest[i] = offsrc[i];
}

void send20(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny*(nz - 1) + nx - 1];
}

void send21(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*ny*(nz - 1)];
  for (i = 0; i < ny; i++)
    dest[i] = offsrc[i*nx];
}

void send22(int nx, int ny, int nz, double *src, double *dest)
{
  double *offsrc = &src[nx*ny*(nz - 1)];
  memcpy(dest, offsrc, nx*ny*sizeof(double));
}

void send23(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*ny*(nz - 1) + nx - 1];
  for (i = 0; i < ny; i++)
    dest[i] = offsrc[i*nx];
}

void send24(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny*(nz - 1) + nx*(ny - 1)];
}

void send25(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*(ny - 1) + nx*ny*(nz - 1)];
  for (i = 0; i < nx; i++)
    dest[i] = offsrc[i];
}

void send26(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny*nz - 1];
}

void recv1(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[0] = src[0];
  } else {
    dest[0] += src[0];
  }
}

void recv2(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  if (type) {
    for (i = 0; i < nx; i++)
      dest[i] = src[i];
  } else {
    for (i = 0; i < nx; i++)
      dest[i] += src[i];
  }
}

void recv3(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx - 1] = src[0];
  } else {
    dest[nx - 1] += src[0];
  }
}

void recv4(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  if (type) {
    for (i = 0; i < ny; i++)
      dest[i*nx] = src[i];
  } else {
    for (i = 0; i < ny; i++)
      dest[i*nx] += src[i];
  }
}

void recv5(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  if (type) {
    for (i = 0; i < nx*ny; i++)
      dest[i] = src[i];
  } else {
    for (i = 0; i < nx*ny; i++)
      dest[i] += src[i];
  }
}

void recv6(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx - 1];

  if (type) {
    for (i = 0; i < ny; i++)
      offdest[i*nx] = src[i];
  } else {
    for (i = 0; i < ny; i++)
      offdest[i*nx] += src[i];
  }
}
void recv7(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*(ny - 1)] = src[0];
  } else {
    dest[nx*(ny - 1)] += src[0];
  }
}

void recv8(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*(ny - 1)];

  if (type) {
    for (i = 0; i < nx; i++)
      offdest[i] = src[i];
  } else {
    for (i = 0; i < nx; i++)
      offdest[i] += src[i];
  }
}

void recv9(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny - 1] = src[0];
  } else {
    dest[nx*ny - 1] += src[0];
  }
}

void recv10(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  if (type) {
    for (i = 0; i < nz; i++)
      dest[i*nx*ny] = src[i];
  } else {
    for (i = 0; i < nz; i++)
      dest[i*nx*ny] += src[i];
  }
}

void recv11(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i, j;
  if (type) {
    for (i = 0; i < nz; i++) {
      for (j = 0; j < nx; j++)
    dest[i*nx*ny + j] = src[i*nx + j];
    }
  } else {
    for (i = 0; i < nz; i++) {
      for (j = 0; j < nx; j++)
    dest[i*nx*ny + j] += src[i*nx + j];
    }
  }
}

void recv12(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx - 1];

  if (type) {
    for (i = 0; i < nz; i++)
      offdest[i*nx*ny] = src[i];
  } else {
    for (i = 0; i < nz; i++)
      offdest[i*nx*ny] += src[i];
  }
}

void recv13(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i, j;
  if (type) {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[i*nx*ny];
      for (j = 0; j < ny; j++)
    offdest[j*nx] = src[i*ny + j];
    }
  } else {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[i*nx*ny];
      for (j = 0; j < ny; j++)
    offdest[j*nx] += src[i*ny + j];
    }
  }
}

void recv14(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i, j;
  if (type) {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[i*nx*ny + nx - 1];
      for (j = 0; j < ny; j++)
    offdest[j*nx] = src[i*ny + j];
    }
  } else {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[i*nx*ny + nx - 1];
      for (j = 0; j < ny; j++)
    offdest[j*nx] += src[i*ny + j];
    }
  }
}

void recv15(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*(ny - 1)];

  if (type) {
    for (i = 0; i < nz; i++)
      offdest[i*nx*ny] = src[i];
  } else {
    for (i = 0; i < nz; i++)
      offdest[i*nx*ny] += src[i];
  }
}

void recv16(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i, j;
  if (type) {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[nx*(ny - 1) + i*nx*ny];
      for (j = 0; j < nx; j++)
    offdest[j] = src[i*nx + j];
    }
  } else {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[nx*(ny - 1) + i*nx*ny];
      for (j = 0; j < nx; j++)
    offdest[j] += src[i*nx + j];
    }
  }
}

void recv17(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*ny - 1];

  if (type) {
    for (i = 0; i < nz; i++)
      offdest[i*nx*ny] = src[i];
  } else {
    for (i = 0; i < nz; i++)
      offdest[i*nx*ny] += src[i];
  }
}

void recv18(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny*(nz - 1)] = src[0];
  } else {
    dest[nx*ny*(nz - 1)] += src[0];
  }
}

void recv19(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*ny*(nz - 1)];

  if (type) {
    for (i = 0; i < nx; i++)
      offdest[i] = src[i];
  } else {
    for (i = 0; i < nx; i++)
      offdest[i] += src[i];
  }
}

void recv20(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny*(nz - 1) + nx - 1] = src[0];
  } else {
    dest[nx*ny*(nz - 1) + nx - 1] += src[0];
  }
}

void recv21(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*ny*(nz - 1)];

  if (type) {
    for (i = 0; i < ny; i++)
      offdest[i*nx] = src[i];
  } else {
    for (i = 0; i < ny; i++)
      offdest[i*nx] += src[i];
  }
}

void recv22(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*ny*(nz - 1)];

  if (type) {
    for (i = 0; i < nx*ny; i++)
      offdest[i] = src[i];
  } else {
    for (i = 0; i < nx*ny; i++)
      offdest[i] += src[i];
  }
}

void recv23(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*ny*(nz - 1) + nx - 1];

  if (type) {
    for (i = 0; i < ny; i++)
      offdest[i*nx] = src[i];
  } else {
    for (i = 0; i < ny; i++)
      offdest[i*nx] += src[i];
  }
}

void recv24(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny*(nz - 1) + nx*(ny - 1)] = src[0];
  } else {
    dest[nx*ny*(nz - 1) + nx*(ny - 1)] += src[0];
  }
}

void recv25(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*(ny - 1) + nx*ny*(nz - 1)];

  if (type) {
    for (i = 0; i < nx; i++)
      offdest[i] = src[i];
  } else {
    for (i = 0; i < nx; i++)
      offdest[i] += src[i];
  }
}

void recv26(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny*nz - 1] = src[0];
  } else {
    dest[nx*ny*nz - 1] += src[0];
  }
}
