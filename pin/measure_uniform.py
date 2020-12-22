#!/usr/bin/env python

import getopt
import glob
import sys

import common

# arguments
datafile = []

# collected data
all_data = {}

def parse_args():
  global datafile

  try:
    opts, _ = getopt.getopt(sys.argv[1:], "d:", ["datafile="])
  except getopt.GetoptError as err:
    print("ERROR:", err)
    sys.exit(1)
  #endtry
  
  for opt, arg in opts:
    if opt in ("-d", "--datafile"):
      datafile += glob.glob(arg)
    #endif
  #endfor
#enddef

def read_data_file(filename, trace_id):
  global all_data

  # translate local to global image uid
  local_to_global = {}

  for line_nr, x in common.read_file_tokens(filename):
    # string input data from external file
    annotation = x[0]

    if annotation == 'IMAGE':
      # process image definition
      local_image_uid = int(x[1])
      image_name = x[2]

      assert local_image_uid not in local_to_global, "ERROR(%s:%d): duplicate local image uid %d" % (filename, line_nr, local_image_uid)
      local_to_global[local_image_uid] = common.get_image_uid(image_name)
    
    elif annotation == 'JUMPSEC':
      # process jump definition
      source_local_image_uid = int(x[1])
      source_section = x[2]
      source_offset = common.signed64(int(x[3], 16))
      source_taken = int(x[4])
      # source_type = int(x[5])

      previous_local_image_uid = int(x[6])
      previous_section = x[7]
      previous_offset = common.signed64(int(x[8], 16))
      previous_taken = int(x[9])
      # previous_type = int(x[10])

      # counter = int(x[11])
      counter = 1

      # convert possible '-1' to hex
      source_offset = common.INVALID_ADDRESS if (source_offset == -1) else source_offset
      previous_offset = common.INVALID_ADDRESS if (previous_offset == -1) else previous_offset

      # translate local to global
      assert source_local_image_uid in local_to_global, "ERROR(%s:%d): unknown local image uid %d" % (filename, line_nr, source_local_image_uid)
      source_image_uid = local_to_global[source_local_image_uid]

      # previous jump can be unknown -> INVALID_IMAGE_UID
      assert (previous_local_image_uid in local_to_global) or (previous_local_image_uid == common.INVALID_IMAGE_UID), "ERROR(%s:%d): unknown local image uid %d" % (filename, line_nr, destination_local_image_uid)
      previous_image_uid = local_to_global[previous_local_image_uid] if previous_local_image_uid != common.INVALID_IMAGE_UID else common.INVALID_IMAGE_UID

      # sanity check
      if previous_image_uid == common.INVALID_IMAGE_UID:
        assert previous_offset == common.INVALID_ADDRESS
      #endif

      encoded_previous = common.INVALID_ADDRESS
      if previous_image_uid != common.INVALID_IMAGE_UID:
        encoded_previous = common.encode_jump(previous_offset, previous_taken, 0, previous_image_uid)

        if previous_section == '.plt':
          encoded_previous = common.INVALID_ADDRESS

        elif (previous_image_uid != source_image_uid) or (previous_section != source_section):
          print "WARNING(%s:%d): not in same section, but taking it into account anyway" % (filename, line_nr)
          print "    prev %s(%s)+0x%x [%s]" % (common.get_image_name(previous_image_uid), previous_section, previous_offset, "taken" if previous_taken else "not taken")
          print "  ->jump %s(%s)+0x%x [%s]" % (common.get_image_name(source_image_uid), source_section, source_offset, "taken" if source_taken else "not taken")
        #endif
      #endif

      # jump_location = (source_image_uid, source_section)
      jump_location = common.get_image_name(source_image_uid)
      if jump_location not in all_data:
        all_data[jump_location] = {}
      
      if source_offset not in all_data[jump_location]:
        all_data[jump_location][source_offset] = {
          # count (not-)taken occurrences of this jump
          'nr_taken': 0,
          'nr_not_taken': 0,

          # data on the preceding jumps that result in a (not-)taken action
          'prevs_taken': {},
          'prevs_not_taken': {}
        }
      #endif

      # total statistics for this jump
      all_data[jump_location][source_offset]['nr_taken' if source_taken else 'nr_not_taken'] += counter

      # data on the preceding jump
      previous_selector = 'prevs_%s' % ('taken' if source_taken else 'not_taken')
      if encoded_previous not in all_data[jump_location][source_offset][previous_selector]:
        all_data[jump_location][source_offset][previous_selector][encoded_previous] = 0
      all_data[jump_location][source_offset][previous_selector][encoded_previous] += counter

    else:
      assert False, "ERROR(%s:%d): unsupported annotation %s" % (filename, line_nr, annotation)
  #endfor
#enddef

def print_results(title, data):
  print >> sys.stderr, "=== %s" % title
  print >> sys.stderr, "  number of conditional jumps  : %d (%.2f %% of total)" % (data['nr_conditional_jumps'], 100.0*data['nr_conditional_jumps']/total_data['nr_conditional_jumps'])
  print >> sys.stderr, "    of which are bidirectional : %d (%.2f %%)" % (data['nr_bidirectional_jumps'], 100.0*data['nr_bidirectional_jumps']/data['nr_conditional_jumps'])
  print >> sys.stderr, "    of which are unidirectional: %d (%.2f %%)" % (data['nr_unidirectional_jumps'], 100.0*data['nr_unidirectional_jumps']/data['nr_conditional_jumps'])

  frac = 0.0
  if data['nr_bidirectional_jumps'] != 0:
    frac = 100.0*data['nr_fixed_path_jumps']/data['nr_bidirectional_jumps']
  print >> sys.stderr, "  fixed paths: %d (%.2f %% of the conditional jumps, %.2f %% of the bidirectional jumps)" % (data['nr_fixed_path_jumps'], 100.0*data['nr_fixed_path_jumps']/data['nr_conditional_jumps'], frac)
  print >> sys.stderr, ""
#enddef

# argument parsing and value verification
parse_args()

assert len(datafile) > 0, "ERROR: no data file provided"
print("INFO: reading data files %s" % datafile)

# read collected data
trace_id = 0
for filename in datafile:
  print("INFO: reading trace %d from %s" % (trace_id, filename))

  read_data_file(filename, trace_id)
  trace_id += 1
#endfor

total_data = {
  'nr_conditional_jumps': 0,
  'nr_unidirectional_jumps': 0,
  'nr_bidirectional_jumps': 0,
  'nr_fixed_path_jumps': 0
}

location_data = {}

# statistics per component
for location, data_for_location in all_data.items():
  location_data[location] = {
    'nr_conditional_jumps': 0,
    'nr_unidirectional_jumps': 0,
    'nr_bidirectional_jumps': 0,
    'nr_fixed_path_jumps': 0
  }

  for jump, data in data_for_location.items():
    # count the total number of conditional jumps
    total_data['nr_conditional_jumps'] += 1
    location_data[location]['nr_conditional_jumps'] += 1

    # count the number of conditional jumps that go in a single direction only
    #   (we're not interesten in processing those jumps further)
    if (data['nr_taken'] == 0) or (data['nr_not_taken'] == 0):
      total_data['nr_unidirectional_jumps'] += 1
      location_data[location]['nr_unidirectional_jumps'] += 1
      continue

    # count the number of conditional jumps that go in both directions
    total_data['nr_bidirectional_jumps'] += 1
    location_data[location]['nr_bidirectional_jumps'] += 1

    nr_to_taken_without_prev = data['prevs_taken'][common.INVALID_ADDRESS] if (common.INVALID_ADDRESS in data['prevs_taken']) else 0
    prevs_to_taken = set(data['prevs_taken'].keys())
    prevs_to_taken.discard(common.INVALID_ADDRESS)

    nr_to_not_taken_without_prev = data['prevs_not_taken'][common.INVALID_ADDRESS] if (common.INVALID_ADDRESS in data['prevs_not_taken']) else 0
    prevs_to_not_taken = set(data['prevs_not_taken'].keys())
    prevs_to_not_taken.discard(common.INVALID_ADDRESS)

    print "candidate 0x%x: %d dynamic taken (%d without previous jump), %d dynamic not-taken (%d without previous jump)" % (jump, data['nr_taken'], nr_to_taken_without_prev, data['nr_not_taken'], nr_to_not_taken_without_prev)

    overlap = prevs_to_taken.intersection(prevs_to_not_taken)
    if len(overlap) == 0:
      print "  no overlap on paths for 0x%x (%d static to taken, %d static to not taken)" % (jump, len(prevs_to_taken), len(prevs_to_not_taken))
      total_data['nr_fixed_path_jumps'] += 1
      location_data[location]['nr_fixed_path_jumps'] += 1
    #endif
  #endfor
#endfor

# sort locations by number of conditional jumps
to_sort = []
for location, data in location_data.items():
  to_sort.append([location, data])

# print totals
print_results("SUMMARY", total_data)

# print numbers per library
sorted_data = sorted(to_sort, key=lambda x: x[1]['nr_conditional_jumps'], reverse=True)
for location, data in sorted_data:
  print_results(location, data)
