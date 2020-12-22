#!/usr/bin/env python

# ./uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err

import bisect
import getopt
import sys
import subprocess
import glob
import common

# arguments
datafile = []
sectionfile = []

SEC_IMAGE_IDX = 0
SEC_NAME_IDX = 1
SEC_START_IDX = 2
SEC_END_IDX = 3
SEC_THREADID_IDX = 4

# collected data
all_data = {}
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

def parse_args():
  global datafile, sectionfile

  try:
    opts, _ = getopt.getopt(sys.argv[1:], "d:s:", ["datafile=", "sectionfile="])
  except getopt.GetoptError as err:
    print("ERROR:", err)
    sys.exit(1)
  #endtry
  
  for opt, arg in opts:
    if opt in ("-d", "--datafile"):
      datafile += glob.glob(arg)
    elif opt in ("-s", "--sectionfile"):
      sectionfile += glob.glob(arg)
    #endif
  #endfor
#enddef

def print_sec(sec, include_address=True):
  return "%s%s(%s)%s" % (("[%d]" % sec[SEC_THREADID_IDX]) if include_address else "", sec[SEC_IMAGE_IDX], sec[SEC_NAME_IDX], (":0x%x-0x%x" % (sec[SEC_START_IDX], sec[SEC_END_IDX]) if include_address else ""))

def binary_address(sec, offset):
  section_str = print_sec(sec, False)

  if sec[SEC_IMAGE_IDX] == '[vdso]':
    return offset

  if section_str not in image_starts:
    command = ['sh', '-c', "readelf -SW %s | egrep '\s%s\s' | sed -r 's:\s+: :g' | rev | cut -d' ' -f7 | rev | sed -r 's:^0+::'" % (sec[SEC_IMAGE_IDX], sec[SEC_NAME_IDX].replace('.', '\\.'))]
    print >> sys.stderr, "looking for %s with command %s" % (section_str, command)
    txt = subprocess.Popen(command, stdout=subprocess.PIPE).communicate()[0].strip()
    nr = int(txt, 16)
    print >> sys.stderr, "'%s': 0x%x," % (section_str, nr)

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

def read_section_file(filename):
  global all_sections

  for line_nr, x in common.read_file_tokens(filename):
    thread_id = int(x[0])
    image = x[1]
    # is_main_image = int(x[2])
    section_name = x[3]
    # [start, end] (both inclusive!)
    section_start = common.signed64(int(x[4], 16))
    section_end = common.signed64(int(x[5], 16))

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
  for line_nr, x in common.read_file_tokens(filename):
    # string input data from external file
    # - cond_branch.so: source=current jump, destination=previous jump
    # - pairs.so: source=control-flow instruction, destination=target address
    source = common.signed64(int(x[0], 16)) & common.MAX_ADDRESS
    source_taken = int(x[1])
    source_type = int(x[2])

    destination = common.signed64(int(x[3], 16)) & common.MAX_ADDRESS
    destination_taken = int(x[4])
    destination_type = int(x[5])

    counter = int(x[6])

    # print("L%d: 0x%x -> 0x%x" % (line_nr, source, destination))

    def locate(virtual_address):
      if virtual_address == common.MAX_ADDRESS:
        return None, -1, ''

      sec = locate_address(virtual_address)
      assert sec is not None, "ERROR(%s:%d): 0x%x not in any section" % (filename, line_nr, jump)
      return sec[SEC_IMAGE_IDX], binary_address(sec, virtual_address - sec[SEC_START_IDX]), sec[SEC_NAME_IDX]
    #enddef

    source_image, source_offset, source_section = locate(source)
    source_image_uid = common.get_image_uid(source_image) if source_image is not None else common.INVALID_IMAGE_UID
    encoded_source = common.encode_jump(source_offset, source_taken, source_type)
    source_key = (source_image_uid, source_section)
    # print("  source: %s 0x%x %s %d" % (source_image, source_offset, source_section, source_taken))

    destination_image, destination_offset, destination_section = locate(destination)
    destination_image_uid = common.get_image_uid(destination_image) if destination_image is not None else common.INVALID_IMAGE_UID
    encoded_destination = common.encode_jump(destination_offset, destination_taken, destination_type)
    destination_key = (destination_image_uid, destination_section)
    # print("  destination: %s 0x%x %s %d %d 0x%x" % (destination_image, destination_offset, destination_section, destination_taken, destination_type, encoded_destination))

    if source_key not in all_data:
      all_data[source_key] = {}
    if encoded_source not in all_data[source_key]:
      all_data[source_key][encoded_source] = {}

    if destination_image not in all_data[source_key][encoded_source]:
      all_data[source_key][encoded_source][destination_key] = set()

    all_data[source_key][encoded_source][destination_key].add((encoded_destination, counter))
  #endfor
#enddef

# argument parsing and value verification
parse_args()

assert len(datafile) > 0, "ERROR: no data file provided"
print >> sys.stderr, "INFO: reading data files %s" % datafile

assert len(sectionfile) > 0, "ERROR: no section file provided"
print >> sys.stderr, "INFO: reading section file %s" % sectionfile

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

for img, img_uid in common._all_images.items():
  print "IMAGE,%d,%s" % (img_uid, img)

for source_img_uid_and_section, source_img_data in all_data.items():
  for source_encoded, source_offset_data in source_img_data.items():
    for destination_img_uid_and_section, destination_img_data in source_offset_data.items():
      for destination_encoded, counter in destination_img_data:
        source_img_uid = source_img_uid_and_section[0]
        source_section = source_img_uid_and_section[1]
        source_offset, source_taken, source_type, _ = common.decode_jump(source_encoded) if source_img_uid != common.INVALID_IMAGE_UID else (common.INVALID_ADDRESS, 0, 0)

        destination_img_uid = destination_img_uid_and_section[0]
        destination_section = destination_img_uid_and_section[1]
        destination_offset, destination_taken, destination_type, _ = common.decode_jump(destination_encoded) if destination_img_uid != common.INVALID_IMAGE_UID else (common.INVALID_ADDRESS, 0, 0, None)

        print "JUMPSEC,%d,%s,0x%x,%d,%d,%d,%s,0x%x,%d,%d,%d" % (source_img_uid, source_section, source_offset, source_taken, source_type, destination_img_uid, destination_section, destination_offset, destination_taken, destination_type, counter)
      #endfor
    #endfor
  #endfor
#endfor
