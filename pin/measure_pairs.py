#!/usr/bin/env python

# /build/pin-3.16-98275-ge0db48c31-gcc-linux/pin -t /data/BOEK/evaluation/metaapi/pin/plugin/cond_branch.so -- /bin/ls > log
# /data/BOEK/evaluation/metaapi/pin/measure.py -d ./data.log -s ./sections.log > measure.log 2> measure.stat
# /data/BOEK/evaluation/metaapi/pin/measure.py -d './data_*.log' -s './sections_*.log' > measure.log 2> measure.stat

import bisect
import getopt
import sys
import subprocess
import glob
import itertools

# operating modes
MODE_PAIRS = 0
MODE_SOURCE = 1
MODE_DESTINATION = 2
MODE_DIRECTION = 3

# arguments
datafile = []
mode = MODE_PAIRS

# collected data
all_data = {}

def signed64(x):
  return -(x & 1<<63) | (x & ~(1<<63))

def parse_args():
  global datafile, mode

  try:
    opts, _ = getopt.getopt(sys.argv[1:], "d:m:", ["datafile=", "mode="])
  except getopt.GetoptError as err:
    print("ERROR:", err)
    sys.exit(1)
  #endtry
  
  for opt, arg in opts:
    if opt in ("-d", "--datafile"):
      datafile += glob.glob(arg)
    elif opt in ("-m", "--mode"):
      mode = int(arg)
      assert (mode == MODE_PAIRS) or (mode == MODE_SOURCE) or (mode == MODE_DESTINATION) or (mode == MODE_DIRECTION), "invalid mode %d" % mode
    #endif
  #endfor
#enddef

def read_file_tokens(filename):
  print("INFO: reading %s" % (filename))

  line_nr = 0
  for line in open(filename):
    line_nr += 1
    line = line.strip()
    if line[0] == '#':
      continue

    yield line_nr, line.split(',')
  #endfor
#enddef

image_uid = 0
all_images = {}
def get_image_uid(image):
  global image_uid, all_images

  if image not in all_images:
    all_images[image] = image_uid
    image_uid += 1

  return all_images[image]
#enddef

def read_data_file(filename, trace_id):
  global all_data

  # translate local to global image uid
  local_to_global = {}

  for line_nr, x in read_file_tokens(filename):
    # string input data from external file
    annotation = x[0]

    if annotation == 'IMAGE':
      # process image definition
      local_image_uid = int(x[1])
      image_name = x[2]

      assert local_image_uid not in local_to_global, "ERROR(%s:%d): duplicate local image uid %d" % (filename, line_nr, local_image_uid)
      local_to_global[local_image_uid] = get_image_uid(image_name)
    
    elif annotation == 'JUMPSEC':
      # process jump definition
      source_local_image_uid = int(x[1])
      source_section = x[2]
      source_offset = signed64(int(x[3], 16))
      # source_taken = int(x[4])
      # source_type = int(x[5])

      destination_local_image_uid = int(x[6])
      destination_section = x[7]
      destination_offset = signed64(int(x[8], 16))
      # destination_taken = int(x[9])
      # destination_type = int(x[10])

      # translate local to global
      assert source_local_image_uid in local_to_global, "ERROR(%s:%d): unknown local image uid %d" % (filename, line_nr, source_local_image_uid)
      assert destination_local_image_uid in local_to_global, "ERROR(%s:%d): unknown local image uid %d" % (filename, line_nr, destination_local_image_uid)

      # don't put 'source_taken' inside the tuple here, as this will cause the same source address to have different keys in the dictionary (the tuple is hashed)

      # source = (local_to_global[source_local_image_uid], source_offset, source_taken, source_type)
      source = (local_to_global[source_local_image_uid], source_section, source_offset)
      # destination = (local_to_global[destination_local_image_uid], destination_offset, destination_taken, destination_type)
      destination = (local_to_global[destination_local_image_uid], destination_section, destination_offset)

      # special cases where we only want to look at one of two sides
      if mode == MODE_SOURCE:
        destination = -1
      elif mode == MODE_DESTINATION:
        source = -1

      if source not in all_data:
        all_data[source] = {}

      if destination not in all_data[source]:
        all_data[source][destination] = set([trace_id])
      else:
        all_data[source][destination].add(trace_id)
    else:
      assert False, "ERROR(%s:%d): unsupported annotation %s" % (filename, line_nr, annotation)
  #endfor
#enddef

def count_not_in(considered_traces):
  print("INFO: count missed for %s" % considered_traces)

  total = 0
  selected = 0

  for _, destinations_for_source in all_data.items():
    for _, traces in destinations_for_source.items():
      if len(traces.intersection(considered_traces)) == 0:
        selected += 1

      total += 1
    #endfor
  #endfor

  return selected, total, 100.0*selected/total
#enddef

def count_direction(considered_traces):
  print("INFO: count direction for %s" % considered_traces)

  total = 0
  selected = 0

  for _, destinations_for_source in all_data.items():
    nr_considered_directions = 0
    nr_all_directions = 0

    # count number of directions this jump takes in the considered traces and in all traces
    for _, traces in destinations_for_source.items():
      if len(traces.intersection(considered_traces)) > 0:
        # source reaches this destination in at least one of the considered traces
        nr_considered_directions += 1
      #endif
      
      nr_all_directions += 1
    #endfor

    if (nr_considered_directions == 1) and (nr_all_directions > 1):
      selected += 1
    
    if nr_all_directions > 1:
      total += 1
  #endfor

  return selected, total, 100.0*selected/total
#enddef

# argument parsing and value verification
parse_args()

assert len(datafile) > 0, "ERROR: no data file provided"
print("INFO: reading data files %s" % datafile)

print("INFO: mode %d" % mode)

# read collected data
trace_id = 0
for filename in datafile:
  print("INFO: reading trace %d from %s" % (trace_id, filename))

  read_data_file(filename, trace_id)
  trace_id += 1
#endfor

trace_combinations = []
for i in range(1, trace_id+1):
  for j in itertools.combinations(range(trace_id), i):
    trace_combinations.append(set(j))

# function to be called
count_function = count_not_in
if mode == MODE_DIRECTION:
  count_function = count_direction

# count not in 1 trace, but in one or more others
for i in trace_combinations:
  _, _, x = count_function(i)
  print("      %s: %.2f %%" % (i, x))

  print >> sys.stderr, "%d,%f" % (len(i), x)
#endfor
