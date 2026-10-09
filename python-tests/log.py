import numpy as np

def append_calculation_step_log(file_path: str, step_formula: str, result: np.ndarray, fmt="%.3f"):
    if file_path is None:
        return
    with open(file_path, "a") as file:
        file.write(step_formula + "\n")
        np.savetxt(file, result, fmt=fmt)
        file.write("\n")


def append_log_line(file_path: str, line: str):
    if file_path is None:
            return
    with open(file_path, "a") as file:
        file.write(line + '\n')


def clear_logfile(file_path):
    if file_path is None:
            return
    with open(file_path, "w") as file:
        file.write("")