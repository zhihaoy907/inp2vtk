#ifndef INP2UCD_H  
#define INP2UCD_H
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <map>
#include <sstream>

class Abaqus_to_UCD
{
  public:
    Abaqus_to_UCD();

    void read_in_abaqus(std::istream &in);
    void write_out_avs_ucd(std::ostream &out) const;

  private:
    const double tolerance;
  
    std::vector<double>
    get_global_node_numbers(const int face_cell_no,const int face_cell_face_no) const;

    std::vector<std::vector<double>> node_list;

    std::vector<std::vector<double>> cell_list;

    std::vector<std::vector<double>> face_list;

    std::map<std::string, std::vector<int>> elsets_list;
  };

template <class T>
bool from_string(T &t, const std::string &s, std::ios_base &(*f)(std::ios_base &));

int extract_int(const std::string &s);

#endif