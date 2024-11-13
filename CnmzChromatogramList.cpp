#include "CnmzChromatogramList.h"

using namespace std;

CnmzChromatogramList::CnmzChromatogramList(){
  count=0;
  chromatogramIndex=NULL;
}

void CnmzChromatogramList::write(FILE* f, int tabs, bool iterative){
  //required
  string el = "chromatogramList";
  if (defaultDataProcessingRef.empty()) NMZerrMsg(el, "defaultDataProcessingRef");

  NMZprintTabs(f, tabs);
  fprintf(f, "<chromatogramList");
  if (iterative) {
    fprintf(f, " count=\"");
    fptr = nmzftell(f);
    fprintf(f, "%.10d\"", 0);
  } else fprintf(f, " count=\"%d\"", count);
  fprintf(f, " defaultDataProcessingRef=\"%s\"", defaultDataProcessingRef.c_str());
  fprintf(f, ">\n");

  if (iterative) return;

  int t = tabs;
  if (t>-1) t++;
  if(chromatogramIndex!=NULL) chromatogramIndex->offset.clear();
  for(size_t a=0;a<chromatogram.size();a++) {
    CnmzOffset o;
    o.idRef = chromatogram[a].id;
    o.content = to_string(nmzftell(f));
    chromatogramIndex->offset.push_back(o);
    chromatogram[a].write(f, t, iterative);
  }

  NMZprintTabs(f, tabs);
  fprintf(f, "</chromatogramList>\n");

}

void CnmzChromatogramList::writeUpdate(FILE* f, int num){
  nmzfseek(f, fptr, 0);
  //cout << "Update: " << count << endl;
  fprintf(f, "%.10d", num);
}
