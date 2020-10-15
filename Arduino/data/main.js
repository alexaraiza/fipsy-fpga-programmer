var selectedRow = null; // selected row from file list
var lastStatus = null; // last status displayed on screen

// Files not to be checked before upload
const doNotCheck = ["index.html", "styles.css", "main.js", "favicon.ico"]




/* On page load
Reset user interface (buttons, form, selected row and last status)
Retrieve file list from server and display it */
document.addEventListener("DOMContentLoaded", function() {

  // Reset user interface
  uploadButton.disabled = true;
  programButton.disabled = true;
  uploadForm.reset();
  selectedRow = null;
  lastStatus = null;

  // Send AJAX request to retrieve file list
  const xhr = new XMLHttpRequest();
  const url = "/list";

  xhr.onreadystatechange = function() {
    if (this.readyState === 4 && this.status === 200) {
      array = JSON.parse(this.responseText);
      if (array.length === 0) {
        listStatus.innerHTML = "File list is empty";
      } else {
        fileTable.deleteRow(-1); // delete "Loading file list..."
        for (const file of array) {
          addToList(file.name, file.size);
        }
      }
    }
  };

  xhr.open("GET", url);
  xhr.send();
});


/* On choosing a file to upload
Display file name
Check file (client-side validation) */
uploadForm.addEventListener("change", function() {
  const file = JEDECfile.files[0];
  uploadButton.disabled = true;
  fileName.innerHTML = file.name;
  fileStatus.innerHTML = "";

  // Perform file check
  checkFile(file, fileStatus, uploadButton)
});


/* On clicking the upload button
Upload file to server, display upload status and add file to file list
NO SERVER-SIDE VALIDATION IMPLEMENTED
(User could upload "invalid" files) */
uploadForm.addEventListener("submit", function(e) {
  uploadButton.disabled = true;
  uploadButton.value = "Uploading...";
  if (lastStatus) {
    lastStatus.innerHTML = "";
  }
  const filename = JEDECfile.files[0].name;
  const filesize = JEDECfile.files[0].size;

  e.preventDefault();

  // Send AJAX request to upload file
  const xhr = new XMLHttpRequest();
  const url = "/upload";

  xhr.onreadystatechange = function() {
    if (this.readyState === 4) {

      // Status is success or failure
      if (this.status === 201) {
        uploadStatus.setAttribute("class", "status success");
        uploadStatus.innerHTML = "Uploaded " + filename;

        // Update JEDEC file in list
        if (currentFileRow) {
          currentFileRow.cells[2].innerHTML = filesize + " B";
        }

        // Append JEDEC file to list
        else if (filename.endsWith(".jed")) {
          if (fileTable.children[0].children[1].cells[1].innerHTML === "File list is empty") {
            fileTable.deleteRow(-1);
          }
          addToList(filename, filesize);
        }
      } else {
        uploadStatus.setAttribute("class", "status failure");
        uploadStatus.innerHTML = "Upload failed";
      }

      lastStatus = uploadStatus;

      uploadButton.value = "Upload File";

      // Enable upload button if chosen file passed check (checkFile)
      if (fileStatus.innerHTML === "") {
        uploadButton.disabled = false;
      }
    }
  };

  xhr.open("POST", url);
  xhr.send(new FormData(uploadForm));

  // Check if uploaded file is in list (overwriting)
  let currentFileRow = null;
  for (const row of fileTable.children[0].children) {
    if (row.cells[1].innerHTML === filename) {
      currentFileRow = row;
      break;
    }
  }
});


/* On clicking the "Remove file" button
Remove file from server and from the file list */
function remove(filename) {
  removeButton = document.getElementById("remove" + filename);
  row = removeButton.parentElement.parentElement;

  removeButton.disabled = true;
  removeButton.innerHTML = "Removing...";

  if (lastStatus) {
    lastStatus.innerHTML = "";
    lastStatus = null;
  }

  // Send AJAX request to remove file
  const xhr = new XMLHttpRequest();
  const url = "/remove?filename=" + filename;

  xhr.onreadystatechange = function() {
    if (this.readyState === 4) {
      
      // Status is success or failure
      if (this.status === 200) {
        removeFromList(row);
      } else {
        removeButton.disabled = false;
        removeButton.innerHTML = "Remove file";
      }
    }
  };

  xhr.open("GET", url);
  xhr.send();
}


/* On clicking the "Program device" button
Program device and display program status */
function program(filename) {
  programButton.disabled = true;
  programButton.innerHTML = "Programming...";
  if (lastStatus) {
    lastStatus.innerHTML = "";
  }

  // Send AJAX request to program device
  const xhr = new XMLHttpRequest();
  const url = "/program?filename=" + filename;

  xhr.onreadystatechange = function() {
    if (this.readyState === 4) {

      // Reset program button
      programButton.innerHTML = "Program device";

      // Status is success or failure
      if (this.status === 200) {
        programStatus.setAttribute("class", "status success");
      } else {
        programStatus.setAttribute("class", "status failure");
      }

      // Display status
      programStatus.innerHTML = this.responseText;
      lastStatus = programStatus;

      // Enable program button
      if (selectedRow) {
        programButton.disabled = false;
      }
    }
  };

  xhr.open("GET", url);
  xhr.send();
}


/* On clicking the "Erase device" button
Erase device and display erase status */
function erase() {
  eraseButton.disabled = true;
  eraseButton.innerHTML = "Erasing...";
  if (lastStatus) {
    lastStatus.innerHTML = "";
  }

  // Send AJAX request to erase device
  const xhr = new XMLHttpRequest();
  const url = "/erase";

  xhr.onreadystatechange = function() {
    if (this.readyState === 4) {

      // Reset erase button
      eraseButton.disabled = false;
      eraseButton.innerHTML = "Erase device";

      // Status is success or failure
      if (this.status === 200) {
        eraseStatus.setAttribute("class", "status success");
      } else {
        eraseStatus.setAttribute("class", "status failure");
      }

      // Display status
      eraseStatus.innerHTML = this.responseText;
      lastStatus = eraseStatus;
    }
  };

  xhr.open("GET", url);
  xhr.send();
}


/* On clicking the "Check device ID" button
Check device ID and display it */
function checkID() {
  checkIDButton.disabled = true;
  checkIDButton.innerHTML = "Checking...";
  if (lastStatus) {
    lastStatus.innerHTML = "";
  }

  // Send AJAX request to check device ID
  const xhr = new XMLHttpRequest();
  const url = "/id";

  xhr.onreadystatechange = function() {
    if (this.readyState === 4) {

      // Reset check ID button
      checkIDButton.disabled = false;
      checkIDButton.innerHTML = "Check device ID";

      // Status is success or failure
      if (this.status === 200) {
        checkIDStatus.setAttribute("class", "status info");
        lastStatus = null;
      } else {
        checkIDStatus.setAttribute("class", "status failure");
        lastStatus = checkIDStatus;
      }

      // Display status
      checkIDStatus.innerHTML = this.responseText;
    }
  };

  xhr.open("GET", url);
  xhr.send();
}


/* On clicking the dark button
Switch between light and dark mode */
darkButton.addEventListener("click", function() {
  if (document.body.attributes.mode.nodeValue === "light") {
    document.body.setAttribute("mode", "dark");
  } else {
    document.body.setAttribute("mode", "light");
  }
});




// Add a file to the list given its name and size
function addToList(name, size) {
  // Create row
  const row = fileTable.insertRow(-1);
  row.setAttribute("class", "list__row");

  row.addEventListener("click", selectRow);
  row.addEventListener("mouseenter", enterRow);
  row.addEventListener("mouseleave", leaveRow);

  // Create row cells
  const selectCell = row.insertCell(0);
  selectCell.setAttribute("class", "select-cell");
  const nameCell = row.insertCell(1);
  nameCell.setAttribute("class", "name-cell");
  const sizeCell = row.insertCell(2);
  sizeCell.setAttribute("class", "size-cell");
  const removeCell = row.insertCell(3);
  removeCell.setAttribute("class", "remove-cell");

  selectCell.innerHTML = "<input type=\"radio\">";
  nameCell.innerHTML = name;
  sizeCell.innerHTML = size + " B";
  removeCell.innerHTML = "<button class=\"button remove-button\" onclick=\"remove('" + name + "')\" id=\"remove" + name + "\">Remove file</button>";
}


// Remove a file from the list given its row
function removeFromList(row) {
  row.style.opacity = 0;

  // Row takes 1s to fade out (CSS transition)
  setTimeout(function() {
    row.remove();

    if (selectedRow === row) {
      programButton.disabled = true;
      selectedRow = null;
    }

    // If there are no files in the list, display "File list is empty"
    if (fileTable.rows.length === 1) {
      const row = fileTable.insertRow(-1);
      const select = row.insertCell(0);
      select.setAttribute("class", "select-cell");
      const status = row.insertCell(1);
      status.setAttribute("id", "listStatus");
      status.innerHTML = "File list is empty";
    }
  }, 1000);
}


// On clicking on a row
function selectRow(e) {
  // If the remove button was clicked
  if (e.target.className === "button remove-button") {
    return;
  }

  // Deselect previously selected row
  if (selectedRow) {
    selectedRow.cells[0].children[0].checked = false;
    selectedRow.setAttribute("class", "list__row");
  }

  // Enable program button
  else {
    programButton.disabled = false;
  }

  if (lastStatus) {
    lastStatus.innerHTML = "";
  }
  lastStatus = null;

  // Select row
  this.cells[0].children[0].checked = true;
  this.style.background = "";
  this.setAttribute("class", "list__row selected-row");
  selectedRow = this;
}


// On mouse entering a row
function enterRow() {
  if (selectedRow !== this) {
    this.style.background = getComputedStyle(this).getPropertyValue("--row-hover-color");
  }
}


// On mouse leaving a row
function leaveRow() {
  if (selectedRow !== this) {
    this.style.background = "";
  }
}




/* checkFile checks the chosen file to be uploaded
Displays file status if the check is failed
Enables the upload button if the check is passed */
function checkFile(file, fileStatus, uploadButton) {
  if (doNotCheck.includes(file.name)) {
    uploadButton.disabled = false;
    return;
  }

  if (!file.name.endsWith(".jed")) {
    fileStatus.innerHTML = "File format must be JEDEC (.jed)";
    return;
  }

  if (file.name.length > 30) {
    fileStatus.innerHTML = "File name cannot be longer than 30 characters";
    return;
  }

  const reader = new FileReader();
  reader.readAsText(file);
  reader.onload = function() {
    const file = reader.result;
    var i = 0;

    // Look for STX
    while (file.charCodeAt(i) != 2 && !isNaN(file.charCodeAt(i))) {
      i++;
    }
    // If STX is not found
    if (i >= file.length) {
      fileStatus.innerHTML = "File check failed: could not find starting STX";
      return;
    }
    const stx = i;

    // Look for QF
    while ((file.charCodeAt(i - 3) != 10 || file.charCodeAt(i - 2) != 81 || file.charCodeAt(i - 1) != 70) && !isNaN(file.charCodeAt(i))) {
      i++;
    }
    if (i >= file.length) {
      fileStatus.innerHTML = "File check failed: could not find 'Q' and qualifier 'F'";
      return;
    }

    // Count address digits
    var addressDigits = 0;
    while (file.charCodeAt(i) != 42) {
      addressDigits++;
      i++;
    }

    i++;
    // Check that we're not at the end of the file
    if (isNaN(file.charCodeAt(i))) {
      fileStatus.innerHTML = "File check failed: address delimiter is at end of file";
      return;
    }

    // Look for 'E' for feature row
    while ((file.charCodeAt(i - 2) != 10 || file.charCodeAt(i - 1) != 69) && !isNaN(file.charCodeAt(i))) {
      i++;
    }
    if (i >= file.length) {
      fileStatus.innerHTML = "File check failed: could not find 'E' for feature row";
      return;
    }

    var featurerow = 0; // feature row bit position
    while (file.charCodeAt(i) == 48 || file.charCodeAt(i) == 49) {
      featurerow++;
      i++;
    }
    if (featurerow != 64) {
      fileStatus.innerHTML = "File check failed: feature row is not 8 bytes long";
      return;
    }

    // Go to next line
    while (file.charCodeAt(i) != 48 && file.charCodeAt(i) != 49) {
      i++;
    }
    var feabits = 0; // feabit position
    while (file.charCodeAt(i) == 48 || file.charCodeAt(i) == 49) {
      if (feabits == 9 && file.charCodeAt(i) == 49) {
        fileStatus.innerHTML = "File check failed: SPI port is disabled";
        return;
      }
      feabits++;
      i++;
    }
    if (feabits != 16) {
      fileStatus.innerHTML = "File check failed: feabits is not 2 bytes long";
      return;
    }

    // Go back to STX
    i = stx;

    // Look for 'L'
    while ((file.charCodeAt(i - 2) != 10 || file.charCodeAt(i - 1) != 76) && !isNaN(file.charCodeAt(i))) {
      i++;
    }
    if (i >= file.length) {
      fileStatus.innerHTML = "File check failed: could not find 'L' for fuse table";
      return;
    }

    // Check that address digits are 0
    const limit = i + addressDigits;
    for (i; i < limit; i++) {
      if (file.charCodeAt(i) != 48) {
        fileStatus.innerHTML = "File check failed: fuse table address is not 0";
        return;
      }
    }

    // File check successful
    if (uploadButton.value !== "Uploading...") {
      uploadButton.disabled = false;
    }
  }
}

function chooseFile() {
  JEDECfile.click();
}