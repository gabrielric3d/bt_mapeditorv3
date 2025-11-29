import struct, os
width,height=4,4
pixels=[]
for y in range(height):
    for x in range(width):
        r=(x*64)%256
        g=(y*64)%256
        b=((x+y)*64)%256
        pixels.append((r,g,b))

def quantize332(r,g,b):
    return ((r>>5)<<5)|((g>>5)<<2)|(b>>6)

indices=[quantize332(r,g,b) for (r,g,b) in pixels]

palette=[0]* (256*3)
for r in range(8):
    for g in range(8):
        for b in range(4):
            idx=(r<<5)|(g<<2)|b
            palette[idx*3+0]=int((r*255)/7) if 7 else 0
            palette[idx*3+1]=int((g*255)/7) if 7 else 0
            palette[idx*3+2]=int((b*255)/3) if 3 else 0

min_code_size=8
clear_code=1<<min_code_size
end_code=clear_code+1
code_size=min_code_size+1
max_code=1<<code_size
next_code=end_code+1

dictionary={}

bit_buffer=0
bit_count=0
raw_bytes=bytearray()

def write_code(code, bits):
    global bit_buffer, bit_count, raw_bytes
    bit_buffer |= code<<bit_count
    bit_count += bits
    while bit_count>=8:
        raw_bytes.append(bit_buffer & 0xFF)
        bit_buffer >>=8
        bit_count -=8

write_code(clear_code, code_size)

prefix=indices[0]
for k in indices[1:]:
    key=(prefix<<8)|k
    if key in dictionary:
        prefix = dictionary[key]
    else:
        write_code(prefix, code_size)
        if next_code<4096:
            dictionary[key]=next_code
            next_code+=1
            if next_code==max_code and code_size<12:
                code_size+=1
                max_code <<=1
            elif next_code==4096:
                write_code(clear_code, code_size)
                dictionary.clear()
                code_size=min_code_size+1
                max_code=1<<code_size
                next_code=end_code+1
        else:
            write_code(clear_code, code_size)
            dictionary.clear()
            code_size=min_code_size+1
            max_code=1<<code_size
            next_code=end_code+1
        prefix=k
write_code(prefix, code_size)
write_code(end_code, code_size)
if bit_count>0:
    raw_bytes.append(bit_buffer & 0xFF)

fname='test.gif'
with open(fname,'wb') as f:
    f.write(b'GIF89a')
    f.write(struct.pack('<HH', width, height))
    f.write(bytes([0xF7,0,0]))
    f.write(bytes(palette))
    f.write(bytes([0x21,0xFF,0x0B]))
    f.write(b'NETSCAPE2.0')
    f.write(bytes([0x03,0x01,0x00,0x00,0x00]))
    f.write(bytes([0x21,0xF9,0x04,0x04,0x00,0x00,0x00,0x00]))
    f.write(bytes([0x2C]))
    f.write(struct.pack('<HHHH',0,0,width,height))
    f.write(bytes([0x00]))
    f.write(bytes([min_code_size]))
    i=0
    while i < len(raw_bytes):
        chunk=raw_bytes[i:i+255]
        f.write(bytes([len(chunk)]))
        f.write(chunk)
        i+=255
    f.write(bytes([0x00,0x3B]))
print('wrote', fname, 'size', os.path.getsize(fname))

