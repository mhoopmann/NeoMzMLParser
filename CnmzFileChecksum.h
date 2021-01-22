#ifndef _CNMZFILECHECKSUM_H
#define _CNMZFILECHECKSUM_H

#include "NeoMzMLStructs.h"

#include <string>
#include <vector>

class CnmzFileChecksum {
public:

  void write(FILE* f, int tabs = -1, bool iterative=false);

  std::string content;


private:

};

#endif