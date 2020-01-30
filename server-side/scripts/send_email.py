#!/usr/bin/env python
"""
Very simple email sender in python

Usage:
    ./send_email.py
"""

import config
import smtplib
import logging
from datetime import date
from email import encoders
from email.mime.base import MIMEBase
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText

PATH="****"
FORMAT = '%(asctime)s - %(message)s'

def mail():

    logging.basicConfig(filename=PATH + "log/email.log", level=logging.INFO, format=FORMAT)

    # Email infos
    message = MIMEMultipart()
    message["From"] = config.sender_email
    message["To"] = config.receiver_email
    message["Subject"] = config.subject
    body = "Copia do arquivo CSV do dia."
    message.attach(MIMEText(body, "plain"))

    filename = PATH + "csv-files/" + str(date.today()) + ".csv"
    # Open PDF file in binary mode
    with open(filename, "rb") as attachment:
        # The content type "application/octet-stream" means that a MIME attachment is a binary file
        part = MIMEBase("application", "octet-stream")
        part.set_payload(attachment.read())
        encoders.encode_base64(part)
        part.add_header("Content-Disposition", f"attachment; filename= {filename}")
        # Add attachment to your message and convert it to string
        message.attach(part)
        text = message.as_string()

    try:
        with smtplib.SMTP_SSL(host=config.smtp_server, port=config.port) as server:
            server.login(config.login, config.password)
            server.sendmail(config.sender_email, config.receiver_email, text)
            server.quit()
            logging.info('Email sent!')
    except:
        logging.info('Email not sent!')


if __name__ == "__main__":
    mail()