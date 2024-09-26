#include "inp2ucd.h"
#include <fstream>
#include <iostream>
int main()
{
    std::ifstream input_stream("QuadLobe_Fuel_job.inp");  // 打开文件
    if (!input_stream.is_open()) {
        std::cerr << "could not open file" << std::endl;
        return -1;
    }
    try
    {
        Abaqus_to_UCD inp2ucd;
        inp2ucd.read_in_abaqus(input_stream);
        std::stringstream in_ucd;
        inp2ucd.write_out_avs_ucd(in_ucd);
        std::ofstream ouput_stream("output.ucd");
        ouput_stream << in_ucd.str();
        input_stream.close();
        ouput_stream.close();
    }
    catch(std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        input_stream.close();
        return -1;
    }
}