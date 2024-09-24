import re
import numpy as np
import chardet

offset = [0]

def detect_encoding(file_path):
    with open(file_path, 'rb') as f:
        raw_data = f.read(20000)
        result = chardet.detect(raw_data)
        return result['encoding']

def parse_inp_nodes(lines):
    nodes = {}
    node_start = False
    current_node_id = 1

    for line in lines:
        if line.strip() == "*Node":
            node_start = True
            continue
        if node_start:
            if line.startswith("*"):
                node_start = False
                global offset
                offset.append(current_node_id - 1)
                print(f"offset:{offset}")
                continue
            
            parts = line.split(',')
            try:
                x = float(parts[1].strip())
                y = float(parts[2].strip())
                z = float(parts[3].strip())
                nodes[current_node_id] = (x, y, z)
                current_node_id += 1
            except ValueError:
                continue
            except IndexError:
                continue

    return nodes

def parse_inp_elements(lines):
    elements = []
    element_types = []
    current_element_type = None
    call_time = 0
    for line in lines:
        if line.startswith("*Element, type="):
            if "C3D4" in line:
                current_element_type = 10
            elif "C3D8" in line:
                current_element_type = 12
            elif "C3D10" in line:
                current_element_type = 24
            elif "C3D20" in line:
                current_element_type = 25
            else:
                current_element_type = None
        elif line.startswith("*") and current_element_type is not None:
            call_time += 1
            current_element_type = None
            continue
        elif current_element_type is not None:
            data = line.split(',')
            try:
                connectivity = [int(n.strip()) + offset[call_time] - 1 for n in data[1:]]
                if connectivity:
                    elements.append(connectivity)
                    element_types.append(current_element_type)
            except ValueError:
                continue
            except IndexError:
                continue
    return elements, element_types

def parse_inp_temperatures(lines):
    temperatures = {}
    temp_start = False
    for line in lines:
        if line.lower().startswith("*initial conditions, type=temperature"):
            temp_start = True
            continue
        if line.startswith("*") and temp_start:
            temp_start = False
        if temp_start:
            parts = line.split(',')
            try:
                node_id = int(parts[0].strip())
                temp = float(parts[1].strip())
                temperatures[node_id] = temp
            except ValueError:
                continue
    return temperatures

def write_to_vtk(f_vtk, nodes, elements, element_types, temperatures):
    f_vtk.write("# vtk DataFile Version 4.2\n")
    f_vtk.write("Unstructured Grid Example\n")
    f_vtk.write("ASCII\n")
    f_vtk.write("DATASET UNSTRUCTURED_GRID\n")
    
    # Points
    f_vtk.write(f"POINTS {len(nodes)} double\n")
    for node_id, (x, y, z) in sorted(nodes.items()):
        f_vtk.write(f"{x} {y} {z}\n")

    # Cells
    num_cells = len(elements)
    num_cell_entries = sum(len(e) + 1 for e in elements)
    f_vtk.write(f"CELLS {num_cells} {num_cell_entries}\n")
    for elem in elements:
        f_vtk.write(f"{len(elem)} " + " ".join(map(str, elem)) + "\n")

    # Cell Types
    f_vtk.write(f"CELL_TYPES {num_cells}\n")
    for etype in element_types:
        f_vtk.write(f"{etype}\n")

    # Point Data
    f_vtk.write(f"POINT_DATA {len(nodes)}\n")
    f_vtk.write("SCALARS Temperature float 1\n")
    f_vtk.write("LOOKUP_TABLE default\n")
    node_id_to_temp = dict(temperatures)
    for node_id, (x, y, z) in sorted(nodes.items()):
        temp = node_id_to_temp.get(node_id, 0.0)
        f_vtk.write(f"{temp}\n")

def main():
    inp_fname = r"input.inp"
    vtk_fname = "output.vtk"

    encoding = detect_encoding(inp_fname)
    print(f"Detected encoding: {encoding}")

    with open(inp_fname, 'r', encoding=encoding, errors='replace') as f_inp:
        lines = f_inp.readlines()

    nodes = parse_inp_nodes(lines)
    elements, element_types = parse_inp_elements(lines)
    temperatures = parse_inp_temperatures(lines)

    with open(vtk_fname, 'w') as f_vtk:
        write_to_vtk(f_vtk, nodes, elements, element_types, temperatures)

if __name__ == "__main__":
    main()
