create DATABASE IF NOT EXISTS SenseDB;
use SenseDB;

CREATE TABLE IF NOT EXISTS Senses
(
	id INTEGER AUTO_INCREMENT,
	client_id INTEGER,
	client_address VARCHAR(100),
	sensor_type VARCHAR(100) NOT NULL,
	x_value DOUBLE NOT NULL,
	y_value DOUBLE,
	z_value DOUBLE,
	ts TIMESTAMP,
	PRIMARY KEY (id)
);
