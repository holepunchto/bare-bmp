// Build the 54-byte header of an uncompressed BITMAPINFOHEADER BMP. Every
// field a test might want to invalidate is a parameter; callers append their
// own pixel data.
exports.makeHeader = function makeHeader({
  width,
  height,
  bpp = 32,
  magic = 'BM',
  headerSize = 40,
  compression = 0,
  dataOffset = 54
}) {
  const header = Buffer.alloc(54)

  header.write(magic, 0)
  header.writeUInt32LE(dataOffset, 10)
  header.writeUInt32LE(headerSize, 14)
  header.writeInt32LE(width, 18)
  header.writeInt32LE(height, 22)
  header.writeUInt16LE(1, 26) // planes
  header.writeUInt16LE(bpp, 28)
  header.writeUInt32LE(compression, 30)

  return header
}
