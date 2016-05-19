#include "mscom.h"
#include "winselect.h"
#include "ArrayLength.h"

#include <oleacc.h>

#include <string>
#include <stdio.h>
using namespace std;

int main(int argc, char* argv[])
{
  mozilla::STARegion sta;
  WCHAR caption[256] = {0};
  WCHAR className[256] = {0};
  HWND hwnd = aspk::SelectWindow();
  GetWindowText(hwnd, caption, ArrayLength(caption));
  GetClassName(hwnd, className, ArrayLength(className));
  printf("HWND: %p \"%S\" \"%S\"\n", hwnd, className, caption);
  return 0;
}

