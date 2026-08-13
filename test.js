const test = require('brittle')
const bmp = require('.')
const { makeHeader } = require('./test/helpers')

const sample = require('./test/fixtures/sample.bmp', {
  with: { type: 'binary' }
})

// A whole 1x1 24-bit BMP: header plus one BGR pixel padded to 4 bytes.
const onePixel = Buffer.concat([
  makeHeader({ width: 1, height: 1, bpp: 24 }),
  Buffer.from([0, 0, 255, 0])
])

test('decode .bmp', (t) => {
  t.comment(bmp.decode(sample))
})

test('decode 24-bit BMP', (t) => {
  const result = bmp.decode(onePixel)

  t.is(result.width, 1)
  t.is(result.height, 1)
  t.is(result.data[0], 255) // R
  t.is(result.data[1], 0) // G
  t.is(result.data[2], 0) // B
  t.is(result.data[3], 255) // A
})

test('encode .bmp', (t) => {
  t.comment(bmp.encode(bmp.decode(sample)))
})

test('encode RGBA to BMP', (t) => {
  const buffer = bmp.encode({
    width: 1,
    height: 1,
    data: Buffer.from([255, 0, 0, 255]) // Red pixel
  })

  t.is(buffer[0], 0x42) // 'B'
  t.is(buffer[1], 0x4d) // 'M'

  const result = bmp.decode(buffer)

  t.is(result.width, 1)
  t.is(result.height, 1)
  t.is(result.data[0], 255) // R
  t.is(result.data[1], 0) // G
  t.is(result.data[2], 0) // B
})

test('encodeAnimated throws', (t) => {
  t.exception(() => bmp.encodeAnimated(), /does not support animation/)
})

test('decode rejects a buffer too small for the headers', (t) => {
  t.exception(() => bmp.decode(Buffer.alloc(53)), /file too small/)
})

test('decode rejects a wrong magic number', (t) => {
  t.exception(
    () => bmp.decode(makeHeader({ width: 1, height: 1, magic: 'XX' })),
    /wrong magic number/
  )
})

test('decode rejects a DIB header that is not BITMAPINFOHEADER', (t) => {
  t.exception(
    () => bmp.decode(makeHeader({ width: 1, height: 1, headerSize: 12 })),
    /only BITMAPINFOHEADER supported/
  )
})

test('decode rejects a compressed BMP', (t) => {
  t.exception(
    () => bmp.decode(makeHeader({ width: 1, height: 1, compression: 1 })),
    /only uncompressed format supported/
  )
})

test('decode rejects an unsupported bit depth', (t) => {
  for (const bpp of [1, 8, 16]) {
    t.exception(
      () => bmp.decode(makeHeader({ width: 1, height: 1, bpp })),
      /only 24-bit and 32-bit formats supported/
    )
  }
})

test('decode rejects width that overflows allocation', (t) => {
  // width * height * 4 wraps int32 to 4, pre-fix this caused a heap OOB write
  t.exception(
    () => bmp.decode(makeHeader({ width: 0x40000001, height: 1 })),
    /pixel data exceeds file size/
  )
})

test('decode rejects INT32_MIN height', (t) => {
  // -INT32_MIN is undefined behavior; must be rejected
  t.exception(() => bmp.decode(makeHeader({ width: 1, height: -2147483648 })), /invalid dimensions/)
})

test('decode rejects non-positive width', (t) => {
  for (const width of [0, -1]) {
    t.exception(() => bmp.decode(makeHeader({ width, height: 1 })), /invalid dimensions/)
  }
})

test('decode rejects zero height', (t) => {
  t.exception(() => bmp.decode(makeHeader({ width: 1, height: 0 })), /invalid dimensions/)
})

test('decode rejects pixel data that runs past the buffer', (t) => {
  // A valid header claiming 4x4 pixels, with no pixel data behind it.
  t.exception(() => bmp.decode(makeHeader({ width: 4, height: 4 })), /pixel data exceeds file size/)
})

test('encode rejects non-positive dimensions', (t) => {
  for (const image of [
    { width: 0, height: 1 },
    { width: 1, height: -1 }
  ]) {
    t.exception(() => bmp.encode({ ...image, data: Buffer.alloc(4) }), /invalid dimensions/)
  }
})

test('encode rejects a data buffer smaller than the dimensions', (t) => {
  t.exception(
    () => bmp.encode({ width: 2, height: 2, data: Buffer.alloc(4) }),
    /data buffer too small/
  )
})
