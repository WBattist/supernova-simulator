#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>


using namespace std;
struct StarLayer {
    double radius;
    double density;
    double pressure;
    double mass;
};

int calculate_pressure(double density, double K) {
    return K * pow(density, 5.0 / 3.0);
}

int calculate_gravity(double mass, double radius) {
    return (6.67430e-11 * mass) / (radius * radius);
}