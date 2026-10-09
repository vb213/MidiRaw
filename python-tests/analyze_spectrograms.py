import os
import numpy as np

pattern=[0, 12, 19, 24, 28, 31, 34, 36, 38, 40]
def notename_to_index(note_name):
    note_names = ['c', 'c#', 'd', 'd#', 'e', 'f', 'f#', 'g', 'g#', 'a', 'a#', 'b']
    octave = int(note_name[-1])
    note = note_name[:-1]
    index = note_names.index(note) + (octave - 1) * 12
    return index

print("Note to index mapping:")

#16, 28, 35, 40, 434

for p in pattern:
    print(16 + p)


print(notename_to_index("e2"))