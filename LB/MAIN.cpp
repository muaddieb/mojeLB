
//#pragma managed(push, off)

//#include "headers/Node.h"
#include "CODE/Solver/Solver.h"
#include <ctime>
#include <iostream>

using namespace std;


int main() {
	cout << "!!!Hello World!!!" << endl;

	//omp_set_num_threads(2);

	std::clock_t start;
	double duration;

	start = std::clock();

	Solver solver;
	solver.Run();

	duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
	cout << " \n duration: " << duration << " [s] = " << duration / 60 << " min" << endl;
	//cout << " omp_get_num_threads(): " << omp_get_num_threads() << endl;

	cout << "-------------------------------------THE END---------------------------------\n" << endl;
	system("PAUSE");
	return 0;
}
