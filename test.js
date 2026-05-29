const test = require('brittle')
const bmp = require('.')

test('decode .bmp', (t) => {
  const image = require('./test/fixtures/sample.bmp', {
    with: { type: 'binary' }
  })

  t.comment(bmp.decode(image))
})

test('decode 24-bit BMP', function (t) {
  // Create minimal 1x1 24-bit BMP
  const header = Buffer.alloc(54)

  // File header
  header.write('BM', 0)
  header.writeUInt32LE(58, 2) // file size (54 + 4 padded row)
  header.writeUInt32LE(54, 10) // data offset

  // DIB header
  header.writeUInt32LE(40, 14) // header size
  header.writeInt32LE(1, 18) // width
  header.writeInt32LE(1, 22) // height
  header.writeUInt16LE(1, 26) // planes
  header.writeUInt16LE(24, 28) // bpp

  // Pixel data (BGR + padding)
  const pixel = Buffer.from([0, 0, 255, 0]) // Blue=0, Green=0, Red=255, pad
  const buffer = Buffer.concat([header, pixel])

  const result = bmp.decode(buffer)

  t.is(result.width, 1)
  t.is(result.height, 1)
  t.is(result.data[0], 255) // R
  t.is(result.data[1], 0) // G
  t.is(result.data[2], 0) // B
  t.is(result.data[3], 255) // A
})

test('encode .bmp', (t) => {
  const image = require('./test/fixtures/sample.bmp', {
    with: { type: 'binary' }
  })

  const decoded = bmp.decode(image)

  t.comment(bmp.encode(decoded))
})

test('encode RGBA to BMP', function (t) {
  const rgba = {
    width: 1,
    height: 1,
    data: Buffer.from([255, 0, 0, 255]) // Red pixel
  }

  const buffer = bmp.encode(rgba)

  // Verify BMP magic
  t.is(buffer[0], 0x42) // 'B'
  t.is(buffer[1], 0x4d) // 'M'

  // Decode and verify round-trip
  const result = bmp.decode(buffer)
  t.is(result.width, 1)
  t.is(result.height, 1)
  t.is(result.data[0], 255) // R
  t.is(result.data[1], 0) // G
  t.is(result.data[2], 0) // B
})

test('encodeAnimated throws', function (t) {
  t.exception(() => bmp.encodeAnimated())
})

function makeHeader({ width, height, bpp = 32 }) {
  const header = Buffer.alloc(54)
  header.write('BM', 0)
  header.writeUInt32LE(54, 10) // data offset
  header.writeUInt32LE(40, 14) // dib header size
  header.writeInt32LE(width, 18)
  header.writeInt32LE(height, 22)
  header.writeUInt16LE(1, 26) // planes
  header.writeUInt16LE(bpp, 28)
  return header
}

test('decode rejects width that overflows allocation', function (t) {
  // width * height * 4 wraps int32 to 4, pre-fix this caused a heap OOB write
  t.exception(() => bmp.decode(makeHeader({ width: 0x40000001, height: 1 })))
})

test('decode rejects INT32_MIN height', function (t) {
  // -INT32_MIN is undefined behavior; must be rejected
  t.exception(() => bmp.decode(makeHeader({ width: 1, height: -2147483648 })))
})

test('decode rejects non-positive width', function (t) {
  t.exception(() => bmp.decode(makeHeader({ width: 0, height: 1 })))
  t.exception(() => bmp.decode(makeHeader({ width: -1, height: 1 })))
})

test('decode rejects zero height', function (t) {
  t.exception(() => bmp.decode(makeHeader({ width: 1, height: 0 })))
})

test('encode rejects non-positive dimensions', function (t) {
  t.exception(() =>
    bmp.encode({ width: 0, height: 1, data: Buffer.alloc(4) })
  )
  t.exception(() =>
    bmp.encode({ width: 1, height: -1, data: Buffer.alloc(4) })
  )
})
