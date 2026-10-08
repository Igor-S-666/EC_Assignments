"""
File readers for plotting utilities.
It contains input_file_reader and output_file_reader functions 
that read input and output files respectively.
"""

input_type = list[tuple[int, int, int]]
output_type = tuple[list[int], int, input_type]

def input_file_reader(file_path: str,
                      file_type: str) -> input_type:
    """
    Reads the input file and returns a list of tuples containing the data.
    
    Args:
        file_path (str): The path to the input file.
        file_type (str): The type of the input file.
    Returns:
        list[tuple[int, int, int]]: A list of tuples nodes in (x,y, weight) format
    """
    data = []
    with open(file_path, 'r') as file:
        if(file_type == 'csv'):
            for line in file:
                if not line.strip():
                    continue
                x, y, weight = map(int, line.strip().split(';'))
                data.append((x, y, weight))
        else:
            raise ValueError(f"Unsupported file type: {file_type}")

    return data

def output_file_reader(file_path: str,
                       file_type: str) -> output_type:
    """
    Reads the output file and returns a list of tuples containing the data.
    
    Args:
        file_path (str): The path to the output file.
        file_type (str): The type of the output file.
    Returns:
        output_type: A tuple containing a list of nodes, an integer value, 
        and a list of tuples in (x,y, weight) format
    """
    with open(file_path, 'r') as file:
        if(file_type == 'txt'):
            # each line is "<label>: <value>", as written by AlgorithmBase::save_result_to_file
            lines = [line.split(':', 1)[1].strip() for line in file if line.strip()]
            input_file = lines[0]
            input_file_type = input_file.split('.')[-1]
            input_data = input_file_reader(input_file, input_file_type)
            node_path = list(map(int, lines[1].split()))
            value = int(lines[2])
            return (node_path, value, input_data)
        else:
            raise ValueError(f"Unsupported file type: {file_type}")