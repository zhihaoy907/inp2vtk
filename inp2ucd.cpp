#include "inp2ucd.h"

int spacedim = 3;
int dim = 3;
int vertices_per_cell = 8;
int vertices_per_face = 4;

Abaqus_to_UCD::Abaqus_to_UCD():tolerance(5e-16){}

void Abaqus_to_UCD::read_in_abaqus(std::istream &input_stream)
{
    if(!input_stream)
    {
        std::cout << "Error: Input stream is not valid" << std::endl;
        return;
    }
    std::string line;
    while (std::getline(input_stream, line))
    {
      cont:
        std::transform(line.begin(), line.end(), line.begin(), ::toupper);//将每个字符转化为大写
        
        if (line.compare("*HEADING") == 0 || line.compare(0, 2, "**") == 0 ||
            line.compare(0, 5, "*PART") == 0)
          {
            while (std::getline(input_stream, line))
            {
                if (line[0] == '*')
                  goto cont;
            }
          }
        //将点存到node_list
        else if (line.compare(0, 6, "*NODE") == 0)
        {
          while (std::getline(input_stream, line))
          {
            //跳过*node那一行
            if (line[0] == '*')
              goto cont;
            //x,y,z存储的地方。多一位用作分隔
            std::vector<double> node(spacedim + 1);

            std::istringstream iss(line);
            char               comma;
            //循环存储，将iss数字部分存到node，","存到comma
            for (unsigned int i = 0; i < spacedim + 1; ++i)
              iss >> node[i] >> comma;
              //将node存到node_list
            node_list.push_back(node);
          }
        }
        else if (line.compare(0, 8, "*ELEMENT") == 0 && line.find("TYPE=") != -1)
        {
            //跳过*ELEMENT那一行
            while (std::getline(input_stream, line))
            {
                if (line[0] == '*')
                  goto cont;

                std::istringstream iss(line);
                char               comma;

                //同*Node，存到cell_list
                const unsigned int n_data_per_cell = 9;
                std::vector<double> cell(n_data_per_cell);
                for (unsigned int i = 0; i < n_data_per_cell; ++i)
                  iss >> cell[i] >> comma;

                cell[0] = n_data_per_cell;
                cell_list.push_back(cell);
            }
        }
        else if (line.compare(0, 8, "*SURFACE") == 0)
        {
            const std::string name_key = "NAME=";
            const std::size_t name_idx_start =
            line.find(name_key) + name_key.size();
            std::size_t name_idx_end = line.find(',', name_idx_start);
            if (name_idx_end == std::string::npos)
            {
              name_idx_end = line.size();
            }
            //将line中的所有整数存到 b_indicator
            const int b_indicator = extract_int(
              line.substr(name_idx_start, name_idx_end - name_idx_start));

            while (std::getline(input_stream, line))
            {
                if (line[0] == '*')
                goto cont;

                //转化为大写
                std::transform(line.begin(),line.end(),line.begin(),::toupper);

                // 表面可以从 ELSET 创建，或者直接从单元格创建
                // 如果 elsets_list 包含一个特定名称的键，则引用该 ELSET，否则引用单元格
                std::istringstream iss(line);
                int                el_idx;
                int                face_number;
                char               temp;

                // 获取相关的面，同时考虑元素的方向
                std::vector<double> quad_node_list;
                const std::string   elset_name = line.substr(0, line.find(','));
                //检查elset_name的元素集是否存在于elsets_list中，存在进if
                if (elsets_list.count(elset_name) != 0)
                {
                    // Surface refers to ELSET
                    std::string stmp;
                    iss >> stmp >> temp >> face_number;

                    const std::vector<int> cells = elsets_list[elset_name];
                    for (const int cell : cells)
                    {
                        el_idx = cell;
                        quad_node_list =
                          get_global_node_numbers(el_idx, face_number);
                        quad_node_list.insert(quad_node_list.begin(),
                                              b_indicator);

                        face_list.push_back(quad_node_list);
                    }
                }
                else
                {
                    // Surface refers directly to elements
                    // 表面直接引用元素
                    char comma;
                    iss >> el_idx >> comma >> temp >> face_number;
                    quad_node_list =
                      get_global_node_numbers(el_idx, face_number);
                    quad_node_list.insert(quad_node_list.begin(), b_indicator);

                    face_list.push_back(quad_node_list);
                }
            }
        }
        else if (line.compare(0, 6, "*ELSET") == 0)
        {
            std::string elset_name;
            {
              const std::string elset_key = "*ELSET, ELSET=";
              const std::size_t idx       = line.find(elset_key);
              //获取elset名字
              if (idx != std::string::npos)
                {
                  const std::string comma       = ",";
                  const std::size_t first_comma = line.find(comma);
                  const std::size_t second_comma =
                    line.find(comma, first_comma + 1);
                  const std::size_t elset_name_start =
                    line.find(elset_key) + elset_key.size();
                  elset_name = line.substr(elset_name_start,
                                           second_comma - elset_name_start);
                }
            }

            std::vector<int>  elements;
            const std::size_t generate_idx = line.find("GENERATE");
            //如果是通过指定步长生成的elset
            //形如:*Elset, elset=Cladding, generate
            //      1,  77400,      1
            if (generate_idx != std::string::npos)
            {
                std::getline(input_stream, line);
                std::istringstream iss(line);
                char               comma;
                int                elid_start;
                int                elid_end;
                int elis_step = 1; // Default if case stride not provided

                iss >> elid_start >> comma >> elid_end;
                
                if (iss.rdbuf()->in_avail() != 0)
                  iss >> comma >> elis_step;

                for (int i = elid_start; i <= elid_end; i += elis_step)
                  elements.push_back(i);
                elsets_list[elset_name] = elements;

                std::getline(input_stream, line);
            }
            else
            {
                while (std::getline(input_stream, line))
                {
                    if (line[0] == '*')
                    break;

                    std::istringstream iss(line);
                    char               comma;
                    int                elid;
                    while (!iss.eof())
                    {
                        iss >> elid >> comma;
                        elements.push_back(elid);
                    }
                }

                elsets_list[elset_name] = elements;
            }
            goto cont;
        }
        else if (line.compare(0, 5, "*NSET") == 0)
        {
            while (std::getline(input_stream, line))
            {
                if (line[0] == '*')
                  goto cont;
            }
        }
        else if (line.compare(0, 14, "*SOLID SECTION") == 0)
        {
            const std::string elset_key = "ELSET=";
            const std::size_t elset_start =
              line.find("ELSET=") + elset_key.size();
            const std::size_t elset_end = line.find(',', elset_start + 1);
            const std::string elset_name =
              line.substr(elset_start, elset_end - elset_start);

            const std::string material_key = "MATERIAL=";
            const std::size_t last_equal =
              line.find("MATERIAL=") + material_key.size();
            const std::size_t material_id_start = line.find('-', last_equal);
            int               material_id       = 0;
            
            from_string(material_id,
                        line.substr(material_id_start + 1),
                        std::dec);

            const std::vector<int> &elset_cells = elsets_list[elset_name];
            for (const int elset_cell : elset_cells)
            {
                const int cell_id     = elset_cell - 1;
                cell_list[cell_id][0] = material_id;
            }
        }
    }
    std::cout << "face_list.size:" << face_list.size() << std::endl;
    std::cout << "node_list.size:" << node_list.size() << std::endl;
    std::cout << "cell_list.size:" << cell_list.size() << std::endl;
}

//提取一行数据中的整数
int extract_int(const std::string &s)
{
  std::string tmp;
  for (const char c : s)
  {
    if (isdigit(c) != 0)
    {
      tmp += c;
    }
  }
  
  int number = 0;
  from_string(number, tmp, std::dec);
  return number;
}

template <class T>
bool from_string(T &t, const std::string &s, std::ios_base &(*f)(std::ios_base &))
{
  std::istringstream iss(s);
  return !(iss >> f >> t).fail();
}


std::vector<double> Abaqus_to_UCD::get_global_node_numbers(const int face_cell_no,const int face_cell_face_no)const
{
    std::vector<double> quad_node_list(4);
    
    if (face_cell_face_no == 1)
    {
      quad_node_list[0] = cell_list[face_cell_no - 1][1];
      quad_node_list[1] = cell_list[face_cell_no - 1][4];
      quad_node_list[2] = cell_list[face_cell_no - 1][3];
      quad_node_list[3] = cell_list[face_cell_no - 1][2];
    }
    else if (face_cell_face_no == 2)
    {
      quad_node_list[0] = cell_list[face_cell_no - 1][5];
      quad_node_list[1] = cell_list[face_cell_no - 1][8];
      quad_node_list[2] = cell_list[face_cell_no - 1][7];
      quad_node_list[3] = cell_list[face_cell_no - 1][6];
    }
    else if (face_cell_face_no == 3)
    {
      quad_node_list[0] = cell_list[face_cell_no - 1][1];
      quad_node_list[1] = cell_list[face_cell_no - 1][2];
      quad_node_list[2] = cell_list[face_cell_no - 1][6];
      quad_node_list[3] = cell_list[face_cell_no - 1][5];
    }
    else if (face_cell_face_no == 4)
    {
      quad_node_list[0] = cell_list[face_cell_no - 1][2];
      quad_node_list[1] = cell_list[face_cell_no - 1][3];
      quad_node_list[2] = cell_list[face_cell_no - 1][7];
      quad_node_list[3] = cell_list[face_cell_no - 1][6];
    }
    else if (face_cell_face_no == 5)
    {
      quad_node_list[0] = cell_list[face_cell_no - 1][3];
      quad_node_list[1] = cell_list[face_cell_no - 1][4];
      quad_node_list[2] = cell_list[face_cell_no - 1][8];
      quad_node_list[3] = cell_list[face_cell_no - 1][7];
    }
    else if (face_cell_face_no == 6)
    {
      quad_node_list[0] = cell_list[face_cell_no - 1][1];
      quad_node_list[1] = cell_list[face_cell_no - 1][5];
      quad_node_list[2] = cell_list[face_cell_no - 1][8];
      quad_node_list[3] = cell_list[face_cell_no - 1][4];
    }
    return quad_node_list;
}


void Abaqus_to_UCD::write_out_avs_ucd(std::ostream &output) const
{
  output << "# Abaqus to UCD mesh conversion" << std::endl;
  output << "# Mesh type: AVS UCD" << std::endl;
    
  output << node_list.size() << "\t" << (cell_list.size() + face_list.size())
          << "\t0\t0\t0" << std::endl;

  output.width(16);
  output.precision(8);

  for (const auto &node : node_list)
  {
    //Node number
    output << node[0] << "\t";

    //Node
    output.setf(std::ios::scientific, std::ios::floatfield);
    for (unsigned int jj = 1; jj < spacedim + 1; ++jj)
    {
      // invoke tolerance -> set points close to zero equal to zero
      if (std::abs(node[jj]) > tolerance)
        output << static_cast<double>(node[jj]) << "\t";
      else
        output << 0.0 << "\t";
    }
    if (spacedim == 2)
      output << 0.0 << "\t";

      output << std::endl;
      output.unsetf(std::ios::floatfield);
  }

  //cell
  for (unsigned int ii = 0; ii < cell_list.size(); ++ii)
  {
    output << ii + 1 << "\t" << cell_list[ii][0] << "\t"
            << (dim == 2 ? "quad" : "hex") << "\t";
    for (unsigned int jj = 1; jj < vertices_per_cell + 1;
          ++jj)
      output << cell_list[ii][jj] << "\t";

      output << std::endl;
    }

    //quad
    for (unsigned int ii = 0; ii < face_list.size(); ++ii)
    {
      output << ii + 1 << "\t" << face_list[ii][0] << "\t"
            << (dim == 2 ? "line" : "quad") << "\t";
      for (unsigned int jj = 1; jj < vertices_per_face + 1;
            ++jj)
        output << face_list[ii][jj] << "\t";

      output << std::endl;
    }
  }