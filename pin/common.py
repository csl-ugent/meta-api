import sys

INVALID_ADDRESS = 0xffffffffffffffff
INVALID_IMAGE_UID = -1
MAX_ADDRESS = 0x00007fffffffffff

TAKEN_BIT = 1<<63

CFTYPE_BIT = 56
MAX_CFTYPE = 0xf
CFTYPE_MASK = 0xf<<CFTYPE_BIT

IMAGE_UID_BIT = 47
MAX_IMAGE_UID = 0x1ff
IMAGE_UID_MASK = 0x1ff<<IMAGE_UID_BIT

def signed64(x):
  return -(x & 1<<63) | (x & ~(1<<63))

def read_file_tokens(filename):
  print >> sys.stderr, "INFO: reading %s" % (filename)

  line_nr = 0
  for line in open(filename):
    line_nr += 1
    line = line.strip()
    if line[0] == '#':
      continue

    yield line_nr, line.split(',')
  #endfor
#enddef

_image_uid = 0
_all_images = {}
def get_image_uid(image):
  global _image_uid, _all_images

  if image not in _all_images:
    _all_images[image] = _image_uid
    _image_uid += 1

  return _all_images[image]
#enddef

def get_image_name(image_uid):
  for image_name, uid in _all_images.items():
    if uid == image_uid:
      return image_name
  #endfor

  return None
#enddef

def encode_jump(address, taken, cftype, image_uid=0):
  assert cftype <= MAX_CFTYPE
  assert image_uid <= MAX_IMAGE_UID

  if taken:
    address |= TAKEN_BIT
  
  address |= (cftype << CFTYPE_BIT)
  address |= (image_uid << IMAGE_UID_BIT)

  return address
#enddef

def decode_jump(address):
  taken = True if (address & TAKEN_BIT) else False
  address &= ~TAKEN_BIT

  cftype = (address & CFTYPE_MASK) >> CFTYPE_BIT
  address &= ~CFTYPE_MASK

  image_uid = (address & IMAGE_UID_MASK) >> IMAGE_UID_BIT
  
  return address, taken, cftype, image_uid
#enddef
