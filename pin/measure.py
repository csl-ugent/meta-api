#!/usr/bin/env python

# /build/pin-3.16-98275-ge0db48c31-gcc-linux/pin -t /data/BOEK/evaluation/metaapi/pin/plugin/cond_branch.so -- /bin/ls > log
# /data/BOEK/evaluation/metaapi/pin/measure.py -d ./data.log -s ./sections.log > measure.log 2> measure.stat
# /data/BOEK/evaluation/metaapi/pin/measure.py -d './data_*.log' -s './sections_*.log' > measure.log 2> measure.stat

import bisect
import getopt
import sys
import subprocess
import glob

# arguments
datafile = []
sectionfile = []
prev_path = True

# constants
INVALID_ADDRESS = 0x7fffffffffffffff

SEC_IMAGE_IDX = 0
SEC_NAME_IDX = 1
SEC_START_IDX = 2
SEC_END_IDX = 3
SEC_THREADID_IDX = 4

# collected data
jump_data = {}
all_sections = []
sorted_all_sections = None

# cached data
image_starts = {
# Fill me based on output of this script to make processing faster.
# The 'binary_address' function in this file will compute data that is not
# present in this list, and print entries that can be added to this data cache.
# To collect the data, execute this script and pipe its stdout:
#   egrep "^'" | sort
}

def signed64(x):
  return -(x & 1<<63) | (x & ~(1<<63))

def parse_args():
  global datafile, sectionfile, prev_path

  try:
    opts, _ = getopt.getopt(sys.argv[1:], "d:s:p", ["datafile=", "sectionfile=", "prevpath"])
  except getopt.GetoptError as err:
    print("ERROR:", err)
    sys.exit(1)
  #endtry
  
  for opt, arg in opts:
    if opt in ("-d", "--datafile"):
      datafile += glob.glob(arg)
    elif opt in ("-s", "--sectionfile"):
      sectionfile += glob.glob(arg)
    elif opt in ("-p", "--prevpath"):
      prev_path = False
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

def encode_jump(address, taken):
  return address | ((1<<63) if taken else 0)

def print_sec(sec, include_address=True):
  return "%s%s(%s)%s" % (("[%d]" % sec[SEC_THREADID_IDX]) if include_address else "", sec[SEC_IMAGE_IDX], sec[SEC_NAME_IDX], (":0x%x-0x%x" % (sec[SEC_START_IDX], sec[SEC_END_IDX]) if include_address else ""))

def binary_address(sec, offset):
  section_str = print_sec(sec, False)

  if sec[SEC_IMAGE_IDX] == '[vdso]':
    return offset

  if section_str not in image_starts:
    command = ['sh', '-c', "readelf -SW %s | egrep '\s%s\s' | sed -r 's:\s+: :g' | cut -d' ' -f5 | sed -r 's:^0+::'" % (sec[SEC_IMAGE_IDX], sec[SEC_NAME_IDX].replace('.', '\\.'))]
    print "looking for %s with command %s" % (section_str, command)
    txt = subprocess.Popen(command, stdout=subprocess.PIPE).communicate()[0].strip()
    nr = int(txt, 16)
    print "'%s': 0x%x," % (section_str, nr)

    image_starts[section_str] = nr
  assert section_str in image_starts, "%s not found in image_starts" % section_str
  
  key = section_str if (section_str in image_starts) else "NULL"
  return image_starts[key] + offset
#enddef

# binary search on sorted list of sections
def locate_address(addr):
  index = bisect.bisect_left(sorted_all_sections_starts, addr)
  if (index < len(sorted_all_sections)) and (sorted_all_sections_starts[index] != addr):
    index -= 1

  assert index >= 0, "index %d for address 0x%x in %s" % (index, addr, [print_sec(x) for x in sorted_all_sections])
  assert index < len(sorted_all_sections), "invalid section index %d for 0x%x in %s" % (index, addr, [print_sec(x) for x in sorted_all_sections])

  sec = sorted_all_sections[index]
  assert (sec[SEC_START_IDX] <= addr) and (addr <= sec[SEC_END_IDX]), "0x%x does not fit in section %s" % (addr, print_sec(sec))

  return sec
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

def read_section_file(filename):
  global all_sections

  for line_nr, x in read_file_tokens(filename):
    thread_id = int(x[0])
    image = x[1]
    # is_main_image = int(x[2])
    section_name = x[3]
    # [start, end] (both inclusive!)
    section_start = signed64(int(x[4], 16))
    section_end = signed64(int(x[5], 16))

    # for each binary, PIN reports a nameless section spanning the entire 64-bit address space
    if section_name == '' \
      and section_start == 0 \
      and section_end == -1:
      continue

    assert section_end != -1, "ERROR(%s:%d): section [%d] %s(%s) has invalid start address 0x%x-0x%x" % (filename, line_nr, thread_id, image, section_name, section_start, section_end)

    # sections starting at 0, according to PIN
    if section_name == '.shstrtab' \
      or section_name == '.comment' \
      or section_name == '.symtab' \
      or section_name == '.strtab' \
      or section_name.startswith(".note") \
      or section_name.startswith('.debug') \
      or section_name.startswith(".gnu"):
      continue

    assert section_start > 0, "ERROR(%s:%d): section [%d] %s(%s) has invalid start address 0x%x-0x%x" % (filename, line_nr, thread_id, image, section_name, section_start, section_end)

    # filter out sections that definitely can't contain code
    if section_name == '.interp' \
      or section_name == '.dynsym' \
      or section_name == '.dynstr' \
      or section_name.startswith(".rela.") \
      or section_name.startswith(".got") \
      or section_name == '.rodata' \
      or section_name.startswith('.eh_frame') \
      or section_name == '.dynamic' \
      or section_name.startswith('.data') \
      or section_name == '.bss' \
      or section_name == '.tbss':
      continue

    # check for overlap with already processed sections
    for sec in all_sections:
      assert not ((sec[SEC_START_IDX] <= section_start) and (section_start <= sec[SEC_END_IDX])), "ERROR(%s:%d): start of new section [%d]%s(%s):0x%x-0x%x in existing section %s" % (filename, line_nr, thread_id, image, section_name, section_start, section_end, print_sec(sec))
      assert not ((sec[SEC_START_IDX] <= section_end) and (section_end <= sec[SEC_END_IDX])), "ERROR(%s:%d): end of new section [%d]%s(%s):0x%x-0x%x in existing section %s" % (filename, line_nr, thread_id, image, section_name, section_start, section_end, print_sec(sec))
    #endfor

    # add this section to the global list
    all_sections.append([image, section_name, section_start, section_end, thread_id])
  #endfor
#enddef

def read_data_file(filename):
  for line_nr, x in read_file_tokens(filename):
    # string input data from external file
    jump = signed64(int(x[0], 16))
    jump_taken = int(x[1])
    prev = signed64(int(x[2], 16))
    prev_taken = int(x[3])
    counter = int(x[4])

    # location in virtual memory: section
    sec = locate_address(jump)
    assert sec is not None, "ERROR(%s:%d): 0x%x not in any section" % (filename, line_nr, jump)

    # print("L%d: jump 0x%x (%d) prev 0x%x (%d) %d in %s" % (line_nr, jump, jump_taken, prev, prev_taken, counter, sec))

    # encode the preceeding jump address, but only if it is valid
    encoded_prev = INVALID_ADDRESS
    if prev != INVALID_ADDRESS:
      prev_sec = locate_address(prev)

      # only encode the preceeding jump if its direction needs to be considered according to the command-line arguments
      encoded_prev = encode_jump(prev, prev_taken) if prev_path else prev

      # skip calls redirected by the PLT
      if prev_sec[SEC_NAME_IDX] == '.plt':
        prev = INVALID_ADDRESS
        encoded_prev = INVALID_ADDRESS

      elif sec[SEC_START_IDX] != prev_sec[SEC_START_IDX]:
        jump_offset = jump - sec[SEC_START_IDX]
        jump_absolute = binary_address(sec, jump_offset)

        prev_offset = prev - prev_sec[SEC_START_IDX]
        prev_absolute = binary_address(prev_sec, prev_offset)

        print "WARNING(%s:%d): not in same section, but taking it into account anyway" % (filename, line_nr)
        print "    prev 0x%x [%s] (+0x%x = 0x%x) @ %s" % (prev, "taken" if prev_taken else "not taken", prev-prev_sec[SEC_START_IDX], prev_absolute, print_sec(prev_sec))
        print "  ->jump 0x%x [%s] (+0x%x = 0x%x) @ %s" % (jump, "taken" if jump_taken else "not taken", jump-sec[SEC_START_IDX], jump_absolute, print_sec(sec))
      #endif
    #endif

    # location of jump
    jump_location = sec[SEC_IMAGE_IDX]

    # ensure that we can store the data somewhere
    if jump_location not in jump_data:
      jump_data[jump_location] = {}

    # add data element if not yet present
    if jump not in jump_data[jump_location]:
      jump_data[jump_location][jump] = {
        # total number of times that this jump is TAKEN
        'nr_taken': 0,
        # total number of times that this jump is NOT-TAKEN
        'nr_not_taken': 0,

        # data on the preceding jumps that result in a TAKEN action
        'prevs_taken': {},
        # data on the preceding jumps that result in a NOT-TAKEN action
        'prevs_not_taken': {}
      }
    #endif

    # total statistics for the jump
    jump_data[jump_location][jump]['nr_taken' if jump_taken else 'nr_not_taken'] += counter

    # location to store the prev data depends on
    # the direction of the inspected jump instruction
    prev_selector = 'prevs_%s' % ('taken' if jump_taken else 'not_taken')

    if encoded_prev not in jump_data[jump_location][jump][prev_selector]:
      jump_data[jump_location][jump][prev_selector][encoded_prev] = 0
    jump_data[jump_location][jump][prev_selector][encoded_prev] += counter

    # debug information for origin of jump
    # print "jump 0x%x at %s(%s)+0x%x" % (jump, sec[SEC_IMAGE_IDX], sec[SEC_NAME_IDX], jump-sec[SEC_START_IDX])
  #endfor
#enddef

# argument parsing and value verification
parse_args()
print "INFO: %stake into account taken or not-taken in preceeding jump" % ("don't " if not prev_path else "")

assert len(datafile) > 0, "ERROR: no data file provided"
print("INFO: reading data files %s" % datafile)

assert len(sectionfile) > 0, "ERROR: no section file provided"
print("INFO: reading section file %s" % sectionfile)

# read sections
for filename in sectionfile:
  read_section_file(filename)

# sort the list of sections for quick lookups by binary searching
#   for the binary search: collect list of start addresses
sorted_all_sections = sorted(all_sections, key=lambda x: x[SEC_START_IDX])
sorted_all_sections_starts = [x[SEC_START_IDX] for x in sorted_all_sections]

# read collected data
for filename in datafile:
  read_data_file(filename)

total_data = {
  'nr_conditional_jumps': 0,
  'nr_unidirectional_jumps': 0,
  'nr_bidirectional_jumps': 0,
  'nr_fixed_path_jumps': 0
}

location_data = {}

# statistics per component
for location, data_for_location in jump_data.items():
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

    nr_to_taken_without_prev = data['prevs_taken'][INVALID_ADDRESS] if (INVALID_ADDRESS in data['prevs_taken']) else 0
    prevs_to_taken = set(data['prevs_taken'].keys())
    prevs_to_taken.discard(INVALID_ADDRESS)

    nr_to_not_taken_without_prev = data['prevs_not_taken'][INVALID_ADDRESS] if (INVALID_ADDRESS in data['prevs_not_taken']) else 0
    prevs_to_not_taken = set(data['prevs_not_taken'].keys())
    prevs_to_not_taken.discard(INVALID_ADDRESS)

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
