// Right now this works because of the order we have includes in the files, but
// obviously that's not a good long term solution. I should clean up this file.

// Read line returns a line read. Note that if the line length is greater
// than bufferLen then this won't return the full line.
int readLine(int socketFd, char* buffer, int bufferLen) {
  int index = 0;
  int lastCarriageReturn = 0;
  for (;;) {
    char temp[1];
    int numRead = read(socketFd, temp, 1);
    // In a well written program would this actually error or just return -1???
    if (numRead == -1) {
      perror("Read failure");
      exit(4);
    } else if (numRead == 0) {
      break;
    }
    // Actually do something to test a line-end here...
    if (temp[0] == '\r') {
      // TODO: Handle two carriage returns in a row...
      lastCarriageReturn = 1;
      continue;
    } else if (temp[0] == '\n' && lastCarriageReturn == 1) {
      break;
    }
    if (lastCarriageReturn) {
      buffer[index] = '\r';
      index++;
    }
    lastCarriageReturn = 0;
    buffer[index] = temp[0];
    index++;
    if (index >= bufferLen) {
      break;
    }
  }
  if (index < bufferLen) {
    buffer[index] = '\0';
  }
  return index;
}
