create DATABASE IF NOT EXISTS SenseDB;
use SenseDB;

CREATE TABLE IF NOT EXISTS users
(
	id INTEGER AUTO_INCREMENT,
	client_id INTEGER UNIQUE,
	client_address VARCHAR(100),
	created_ts TIMESTAMP,
	PRIMARY KEY (id)
);

CREATE TABLE IF NOT EXISTS senses
(
	id INTEGER AUTO_INCREMENT,
	user_id INTEGER,
	json VARCHAR(1024),
	ts TIMESTAMP,
	PRIMARY KEY (id),
	FOREIGN KEY (user_id) REFERENCES users(client_id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS anomaly
(
	id INTEGER AUTO_INCREMENT,
	senses_id INTEGER NOT NULL,
	description VARCHAR(100),
	PRIMARY KEY (id),
	FOREIGN KEY (senses_id) REFERENCES senses(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS gps
(
	id INTEGER AUTO_INCREMENT,
	senses_id INTEGER NOT NULL,
	x_value DOUBLE NOT NULL,
	y_value DOUBLE,
	z_value DOUBLE,
	PRIMARY KEY (id),
	FOREIGN KEY (senses_id) REFERENCES senses(id) ON DELETE CASCADE
);

(select u.client_id, 'magnetometer', m.x_value, m.y_value, m.z_value from magnetometer m,senses s, users u where m.senses_id = s.id and s.user_id = u.id);
