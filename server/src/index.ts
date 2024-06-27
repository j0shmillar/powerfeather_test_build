import express from 'express';
import bodyParser from 'body-parser';
import fs from 'fs';
import path from 'path';

const app = express();
const port = 5003;
const imagesDir = path.join(__dirname, 'images');

// Ensure the images directory exists
if (!fs.existsSync(imagesDir)) {
  fs.mkdirSync(imagesDir);
}

app.use(
  bodyParser.raw({
    type: '*/*',
    limit: '50mb'
  })
);

app.post('/samples', (req, res) => {
  const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
  const filename = path.join(imagesDir, `image-${timestamp}.jpg`);

  fs.writeFile(filename, req.body, (err) => {
    if (err) {
      console.error('failed to save image:', err);
      res.status(500).send('error');
    } else {
      console.log(`saved image as ${filename}`);
      res.send('OK');
    }
  });
});

app.get('/', (req, res) => {
  res.send('running...');
});

app.listen(port, '0.0.0.0', () => {
  console.log(`started at http://0.0.0.0:${port}`);
});
