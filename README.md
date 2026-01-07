# bare-bmp

Native BMP codec for Bare.

```
npm install bare-bmp
```

## Usage

```javascript
const bmp = require('bare-bmp')

// Decode BMP to RGBA
const { width, height, data } = bmp.decode(buffer)

// Encode RGBA to BMP
const buffer = bmp.encode({ width, height, data })
```

## License

Apache-2.0
