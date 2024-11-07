const express = require('express');
const { InfluxDB } = require('@influxdata/influxdb-client');
const cors = require('cors');
const path = require('path');

const app = express();
const port = 3040;

// Enable CORS and JSON parsing
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// InfluxDB configuration
const url = 'https://us-east-1-1.aws.cloud2.influxdata.com';
const token = 'NFdWzEBxBFZzurEU4eBUx2J46YKDD6Wj3RCvY9M0nX4MpdV1fI_Xog6IYUdV0Qtru3pqW8KyejVrqCe0gmQaGA==';
const org = 'Amrita';
const bucket = 'gg';
const influxDB = new InfluxDB({ url, token });
const queryApi = influxDB.getQueryApi(org);

// Endpoint to fetch all attendance data
app.get('/api/attendance', async (req, res) => {
  const query = `
    from(bucket: "${bucket}")
    
    |> range(start: -24h) // Adjust the range as needed
    |> filter(fn: (r) => r._measurement == "attendance_records")
    
    |> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")
  `;

  try {
    const attendanceData = [];
    await new Promise((resolve, reject) => {
      queryApi.queryRows(query, {
        next(row, tableMeta) {
          const record = tableMeta.toObject(row);
          attendanceData.push({
            section: record.section,
            roll_number: record.roll_number,
            status: record.status,
            time: new Date(record._time).toLocaleString(),
          });
        },
        error(error) {
          console.error('Query error:', error);
          reject(error);
        },
        complete() {
          resolve();
        },
      });
    });

    res.json({ attendanceData, count: attendanceData.length });
  } catch (error) {
    console.error('Error fetching data:', error);
    res.status(500).json({ error: 'Error fetching attendance data' });
  }
});

// Serve the UI
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.listen(port, () => {
  console.log(`Server running at http://localhost:${port}`);
});