#include "windows_service_base.h"
#include <iostream>

using namespace std;

int main()
{
	windows_service_base base(TEXT("test"));
	windows_service_base::run_service(base);
	return 0;
}