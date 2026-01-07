const binding = require('./binding')

exports.decode = function decode(buffer) {
  const { width, height, data } = binding.decode(buffer)

  return {
    width,
    height,
    data: Buffer.from(data)
  }
}

exports.encode = function encode(image, opts = {}) {
  const buffer = binding.encode(image, opts)

  return Buffer.from(buffer)
}

exports.encodeAnimated = function encodeAnimated() {
  return binding.encodeAnimated()
}
