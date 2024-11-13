#ifndef _CNMZCHROMATOGRAMLIST_H
#define _CNMZCHROMATOGRAMLIST_H

#include "NeoMzMLStructs.h"
#include "CnmzChromatogram.h"
#include "CnmzIndex.h"

#include <string>
#include <vector>

class CnmzChromatogramList {
public:
  CnmzChromatogramList();

  void write(FILE* f, int tabs = -1, bool interative = false);
  void writeUpdate(FILE* f, int num);

  CnmzIndex* chromatogramIndex;
  std::vector<CnmzChromatogram> chromatogram;

  int count;
  std::string defaultDataProcessingRef;


private:

  f_off_nmz fptr;

};

#endif