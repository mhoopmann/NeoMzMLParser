#ifndef _CNMZCHROMATOGRAM_H
#define _CNMZCHROMATOGRAM_H

#include "NeoMzMLStructs.h"
#include "CnmzBinaryDataArrayList.h"
#include "CnmzPrecursorList.h"
#include "CnmzReferenceableParamGroupRef.h"
#include "CnmzUserParam.h"

#include <string>
#include <vector>

class CnmzChromatogram {
public:

  CnmzChromatogram();

  void write(FILE* f, int tabs = -1, bool iterative = false);

  std::vector<CnmzReferenceableParamGroupRef> referencableParamGroupRef;
  std::vector<CnmzCvParam> cvParam;
  std::vector<CnmzUserParam> userParam;
  CnmzBinaryDataArrayList binaryDataArrayList;

  std::string dataProcessingRef;
  int defaultArrayLength;
  std::string id;
  int index;

private:

};

#endif