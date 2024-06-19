import express from 'express';
import bodyParser from 'body-parser';
import fs from 'fs';

const app = express();
const port = 5003;

app.use(
  bodyParser.raw({
    // inflate: true,
    type: '*/*',
    limit: '50mb'
  })
);

app.post('/samples', (req, res) => {
  // tslint:disable-next-line:no-console
  console.log(`Got ${req.body.length} I2S bytes`);
  fs.appendFile('i2s.raw', req.body, () => {
    res.send('OK');
  });
});

app.get('/', (req, res) => {
  res.send('Server is running.');
});

app.listen(port, '0.0.0.0', () => {
  // tslint:disable-next-line:no-console
  console.log(`server started at http://0.0.0.0:${port}`);
});