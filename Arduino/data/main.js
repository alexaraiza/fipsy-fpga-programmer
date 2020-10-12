/* Initialize selectedRow and lastStatus, which keep track of the last row the user selected and the last status displayed on screen
 */
var selectedRow = null;
var lastStatus = null;

// On page load
document.addEventListener("DOMContentLoaded", function(){
  // Reset buttons, upload form, selected row and last displayed status
  uploadButton.disabled = true;
  programButton.disabled = true;
  uploadForm.reset();
  selectedRow = null;
  lastStatus = null;

  // Send AJAX request to retrieve file list
  const xhr = new XMLHttpRequest();
  const url = "/list";

  xhr.onreadystatechange = function(){
    // If response is successful
    if (this.readyState === 4 && this.status === 200){
      // Parse list to get array of files
      array = JSON.parse(this.responseText);
      if (array.length === 0){
        listStatus.innerHTML = "File list is empty";
      }
      else{
        fileTable.deleteRow(-1); // delete "Loading file list..."
        for (const file of array){
          addToList(file.name, file.size);
        }
      }
    }
  };

  xhr.open("GET", url);
  xhr.send();
});

// On choosing a file to upload
uploadForm.addEventListener("change", function(){
  const file = JEDECfile.files[0];
  uploadButton.disabled = true;
  fileName.innerHTML = file.name; // display file name
  fileStatus.innerHTML = ""; // reset file information

  // If file is index.html or favicon.ico, allow upload
  if (file.name === "index.html" || file.name === "favicon.ico"){
    uploadButton.disabled = false;
    return;
  }

  // If file is not JEDEC
  if (!file.name.endsWith(".jed")){
    fileStatus.innerHTML = "File format must be JEDEC (.jed)";
    return;
  }

  // If file name is too long
  if (file.name.length > 30){
    fileStatus.innerHTML = "File name cannot be longer than 30 characters";
    return;
  }

  // checkFile will allow the file to be uploaded or not
  checkFile(file, fileStatus, uploadButton)
});

// On file upload
uploadForm.addEventListener("submit", function(e){
  uploadButton.disabled = true;
  uploadButton.value = "Uploading...";
  if (lastStatus){
    lastStatus.innerHTML = ""; // reset last status
  }
  const filename = JEDECfile.files[0].name;
  const filesize = JEDECfile.files[0].size;

  // Prevent default behavior
  e.preventDefault();

  // Send AJAX request to upload file
  const xhr = new XMLHttpRequest();
  const url = "/upload";
  
  xhr.onreadystatechange = function(){
    if (this.readyState === 4){ // When response is ready
      // If response is successful
      if (this.status === 201){
        uploadStatus.setAttribute("class", "status success");
        uploadStatus.innerHTML = "Uploaded " + filename;

        // Update JEDEC file in list
        if (currentFileRow){
          currentFileRow.cells[2].innerHTML = filesize + " B";
        }

        // Append JEDEC file to list
        else if (filename.endsWith(".jed")){
          if (fileTable.children[0].children[1].cells[1].innerHTML === "File list is empty"){
            fileTable.deleteRow(-1);
          }
          addToList(filename, filesize);
        }
      }

      // If response failed
      else{
        uploadStatus.setAttribute("class", "status failure");
        uploadStatus.innerHTML = "Upload failed";
      }
      
      lastStatus = uploadStatus;

      // Enable upload button if file is correct
      uploadButton.value = "Upload File";
      if (fileStatus.innerHTML === ""){
        uploadButton.disabled = false;
      }
    }
  };

  xhr.open("POST", url);
  xhr.send(new FormData(uploadForm));

  // Check if uploaded file is in list (overwriting)
  let currentFileRow = null;
  for (const row of fileTable.children[0].children){
    if (row.cells[1].innerHTML === filename){
      currentFileRow = row;
      break;
    }
  }
});

darkButton.addEventListener("click", function(){
  if (document.body.attributes.mode.nodeValue === "light"){
    document.body.setAttribute("mode", "dark");
  }
  else{
    document.body.setAttribute("mode", "light");
  }
});

function checkFile(file, fileStatus, uploadButton){
  const reader = new FileReader();
  reader.readAsText(file);
  reader.onload = function(){
    const file = reader.result;
    var i = 0;

    // Look for STX
    while (file.charCodeAt(i) != 2 && !isNaN(file.charCodeAt(i))){
      i++;
    }
    // If STX is not found
    if (i >= file.length){
      fileStatus.innerHTML = "File check failed: could not find starting STX";
      return;
    }
    const stx = i;

    // Look for QF
    while ((file.charCodeAt(i-3) != 10 || file.charCodeAt(i-2) != 81 || file.charCodeAt(i-1) != 70) && !isNaN(file.charCodeAt(i))){
      i++;
    }
    if (i >= file.length){
      fileStatus.innerHTML = "File check failed: could not find 'Q' and qualifier 'F'";
      return;
    }
    
    // Count address digits
    var addressDigits = 0;
    while (file.charCodeAt(i) != 42){
      addressDigits++;
      i++;
    }

    i++;
    // Check that we're not at the end of the file
    if (isNaN(file.charCodeAt(i))){
      fileStatus.innerHTML = "File check failed: address delimiter is at end of file";
      return;
    }

    // Look for 'E' for feature row
    while ((file.charCodeAt(i-2) != 10 || file.charCodeAt(i-1) != 69) && !isNaN(file.charCodeAt(i))){
      i++;
    }
    if (i >= file.length){
      fileStatus.innerHTML = "File check failed: could not find 'E' for feature row";
      return;
    }

    var featurerow = 0; // feature row bit position
    while (file.charCodeAt(i) == 48 || file.charCodeAt(i) == 49){
      featurerow++;
      i++;
    }
    if (featurerow != 64){
      fileStatus.innerHTML = "File check failed: feature row is not 8 bytes long";
      return;
    }

    // Go to next line
    while (file.charCodeAt(i) != 48 && file.charCodeAt(i) != 49){
      i++;
    }
    var feabits = 0; // feabit position
    while (file.charCodeAt(i) == 48 || file.charCodeAt(i) == 49){
      if (feabits == 9 && file.charCodeAt(i) == 49){
        fileStatus.innerHTML = "File check failed: SPI port is disabled";
        return;
      }
      feabits++;
      i++;
    }
    if (feabits != 16){
      fileStatus.innerHTML = "File check failed: feabits is not 2 bytes long";
      return;
    }

    // Go back to STX
    i = stx;

    // Look for 'L'
    while ((file.charCodeAt(i-2) != 10 || file.charCodeAt(i-1) != 76) && !isNaN(file.charCodeAt(i))){
      i++;
    }
    if (i >= file.length){
      fileStatus.innerHTML = "File check failed: could not find 'L' for fuse table";
      return;
    }

    // Check that address digits are 0
    const limit = i + addressDigits;
    for (i; i < limit; i++){
      if (file.charCodeAt(i) != 48){
        fileStatus.innerHTML = "File check failed: fuse table address is not 0";
        return;
      }
    }

    // File check successful
    if (uploadButton.value !== "Uploading..."){
      uploadButton.disabled = false;
    }
  }
}

// On remove file
function remove(filename){
  // Get button and row elements
  removeButton = document.getElementById("remove" + filename);
  row = removeButton.parentElement.parentElement;

  removeButton.disabled = true;
  removeButton.innerHTML = "Removing...";

  /* Reset last status (remove does not display a status: if it succeeds, the row disappears; if not, it resets)
   */
  if (lastStatus){
    lastStatus.innerHTML = "";
    lastStatus = null;
  }

  // Send AJAX request to remove file
  const xhr = new XMLHttpRequest();
  const url = "/remove?filename=" + filename;

  xhr.onreadystatechange = function(){
    if (this.readyState === 4){ // When response is ready
      // If response is successful
      if (this.status === 200){
        if (selectedRow === row){
          programButton.disabled = true;
          selectedRow = null;
        }
        removeFromList(row);
      }

      // If response failed
      else{
        removeButton.disabled = false;
        removeButton.innerHTML = "Remove file";
      }
    }
  };

  xhr.open("GET", url);
  xhr.send();
}

// On program command
function program(filename){
  programButton.disabled = true;
  programButton.innerHTML = "Programming...";
  if (lastStatus){
    lastStatus.innerHTML = "";
  }
      
  // Send AJAX request to program device
  const xhr = new XMLHttpRequest();
  const url = "/program?filename=" + filename;
      
  xhr.onreadystatechange = function(){
    if (this.readyState === 4){ // When response is ready
      programButton.innerHTML = "Program device";

      // If response is successful
      if (this.status === 200){
        programStatus.setAttribute("class", "status success");
      }

      // If response failed
      else{
        programStatus.setAttribute("class", "status failure");
      }

      // Display status
      programStatus.innerHTML = this.responseText;
      lastStatus = programStatus;

      /* If a file is selected (always the case, except when the program command was run immediately after the command to remove the same file, not giving it enough time to receive a response)
       */
      if (selectedRow){
        programButton.disabled = false;
      }
    }
  };

  xhr.open("GET", url);
  xhr.send();
}

// On erase command
function erase(){
  eraseButton.disabled = true;
  eraseButton.innerHTML = "Erasing...";
  if (lastStatus){
    lastStatus.innerHTML = "";
  }

  // Send AJAX request to erase device
  const xhr = new XMLHttpRequest();
  const url = "/erase";

  xhr.onreadystatechange = function(){
    if (this.readyState === 4){ // When response is ready
      eraseButton.disabled = false;
      eraseButton.innerHTML = "Erase device";

      // If response was successful
      if (this.status === 200){
        eraseStatus.setAttribute("class", "status success");
      }

      // If response failed
      else{
        eraseStatus.setAttribute("class", "status failure");
      }
      eraseStatus.innerHTML = this.responseText;
      lastStatus = eraseStatus;
    }
  };

  xhr.open("GET", url);
  xhr.send();
}

// On check ID command
function checkID(){
  checkIDButton.disabled = true;
  checkIDButton.innerHTML = "Checking...";
  if (lastStatus){
    lastStatus.innerHTML = "";
  }

  // Send AJAX request to check device ID
  const xhr = new XMLHttpRequest();
  const url = "/id";

  xhr.onreadystatechange = function(){
    if (this.readyState === 4){ // When response is ready
      checkIDButton.disabled = false;
      checkIDButton.innerHTML = "Check device ID";

      // If response is successful
      if (this.status === 200){
        checkIDStatus.setAttribute("class", "status info");
        lastStatus = null;
      }

      // If response failed
      else{
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

function chooseFile(){
  JEDECfile.click();
}

// On row select (when a row is clicked)
function selectRow(e){
  // If the remove button was clicked
  if (e.target.className === "remove-button"){
    return;
  }

  // Deselect previously selected row
  if (selectedRow){
    selectedRow.cells[0].children[0].checked = false;
    selectedRow.setAttribute("class", "list__row");
  }

  // Enable program button
  else{
    programButton.disabled = false;
  }

  if (lastStatus){
    lastStatus.innerHTML = "";
  }
  lastStatus = null;

  // Select row
  this.cells[0].children[0].checked = true;
  this.removeAttribute("style");
  this.setAttribute("class", "list__row selected-row");
  selectedRow = this;
}

// On mouse enter
function enterRow(){
  if (selectedRow !== this){
    this.style.background = getComputedStyle(this).getPropertyValue("--row-hover-color");
  }
}

// On mouse leave
function leaveRow(){
  if (selectedRow !== this){
    this.removeAttribute("style");
  }
}

// Add a file to the list given its name and size
function addToList(name, size){
  const row = fileTable.insertRow(-1);
  row.setAttribute("class", "list__row");
  
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
  // Generate a remove button
  removeCell.innerHTML = "<button class=\"button remove-button\" onclick=\"remove('" + name + "')\" id=\"remove" + name + "\">Remove file</button>";
  
  row.addEventListener("click", selectRow); // listen for row selection
  row.addEventListener("mouseenter", enterRow); // listen for mouse enter
  row.addEventListener("mouseleave", leaveRow); // listen for mouse leave
}

// Remove a file from the list given its row
function removeFromList(row){
  // Row takes 1s to fade out (CSS transition)
  row.style.opacity = 0;
  setTimeout(function(){
    row.remove();
    // If there are no files in the list, display "File list is empty"
    if (fileTable.rows.length === 1){
      const row = fileTable.insertRow(-1);
      const select = row.insertCell(0);
      select.setAttribute("class", "select-cell");
      const status = row.insertCell(1);
      status.setAttribute("id", "listStatus");
      status.innerHTML = "File list is empty";
    }
  }, 1000);
}