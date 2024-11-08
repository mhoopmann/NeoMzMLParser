#ifndef _CNMZSPECTRUMLIST_H
#define _CNMZSPECTRUMLIST_H

#include "NeoMzMLStructs.h"
#include "CnmzIndex.h"
#include "CnmzSpectrum.h"

#include <string>
#include <vector>

class CnmzSpectrumList {
public:

  CnmzSpectrumList();

  void write(FILE* f, int tabs = -1, bool interative=false);
  void writeUpdate(FILE* f, int num);

  CnmzIndex* spectrumIndex;
  std::vector<CnmzSpectrum> spectrum;

  int count;
  std::string defaultDataProcessingRef;


private:

  f_off_nmz fptr;

};

#endif