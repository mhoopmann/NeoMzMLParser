#include "CnmzChromatogram.h"

using namespace std;

CnmzChromatogram::CnmzChromatogram(){
  defaultArrayLength=-1;
  index=0;
}

void CnmzChromatogram::write(FILE* f, int tabs, bool iterative){
  //required
  string el = "chromatogram";
  if (id.empty()) NMZerrMsg(el, "id");
  if(defaultArrayLength<0) NMZerrMsg(el,"defaultArrayLength");

  NMZprintTabs(f, tabs);
  fprintf(f, "<chromatogram");
  fprintf(f, " index=\"%d\"", index);
  fprintf(f, " id=\"%s\"", id.c_str());
  fprintf(f, " defaultArrayLength=\"%d\"", defaultArrayLength);
  if (!dataProcessingRef.empty()) fprintf(f, " dataProcessingRef=\"%s\"", dataProcessingRef.c_str());
  fprintf(f, ">\n");

  int t = tabs;
  if (t>-1) t++;
  for (size_t a = 0; a<referencableParamGroupRef.size(); a++) referencableParamGroupRef[a].write(f, t, iterative);
  for (size_t a = 0; a<cvParam.size(); a++) cvParam[a].write(f, t, iterative);
  for (size_t a = 0; a<userParam.size(); a++) userParam[a].write(f, t, iterative);
  binaryDataArrayList.write(f, t, iterative);

  NMZprintTabs(f, tabs);
  fprintf(f, "</chromatogram>\n");


}

