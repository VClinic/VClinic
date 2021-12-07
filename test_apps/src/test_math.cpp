#include<stdio.h>
#include<math.h>
void foo(double* A, int n) {
	for(int i=0; i<n; ++i) {
		A[i] = sin(A[i]);
	}
}

void foo2(double* A, int n) {
	for(int i=0; i<n; ++i) {
		A[i] = exp(A[i]);
	}
}

void foo3(double* A, int n) {
	for(int i=0; i<n; ++i) {
		A[i] = cos(A[i]);
	}
}

int main() {
	double* A;
	A = (double*)malloc(sizeof(double)*1000);
	for(int i=0; i<1000; ++i) A[i] = 0;
	foo(A, 1000);
	foo2(A, 1000);
	foo3(A, 1000);
	printf("%lf\n", A[0]);
	return 0;
}
