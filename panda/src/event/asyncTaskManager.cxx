// Filename: asyncTaskManager.cxx
// Created by:  drose (23Aug06)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) Carnegie Mellon University.  All rights reserved.
//
// All use of this software is subject to the terms of the revised BSD
// license.  You should have received a copy of this license along
// with this source code in a file named "LICENSE."
//
////////////////////////////////////////////////////////////////////

#include "asyncTaskManager.h"
#include "event.h"
#include "pt_Event.h"
#include "mutexHolder.h"
#include "indent.h"
#include "pStatClient.h"
#include "pStatTimer.h"
#include "clockObject.h"
#include "config_event.h"
#include <algorithm>

PT(AsyncTaskManager) AsyncTaskManager::_global_ptr;

TypeHandle AsyncTaskManager::_type_handle;

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::Constructor
//       Access: Published
//  Description:
////////////////////////////////////////////////////////////////////
AsyncTaskManager::
AsyncTaskManager(const string &name) :
  Namable(name),
  _lock("AsyncTaskManager::_lock"),
  _num_tasks(0),
  _clock(ClockObject::get_global_clock()),
  _frame_cvar(_lock)
{
  // Make a default task chain.
  do_make_task_chain("default");
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::Destructor
//       Access: Published, Virtual
//  Description:
////////////////////////////////////////////////////////////////////
AsyncTaskManager::
~AsyncTaskManager() {
  cleanup();
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::cleanup
//       Access: Published
//  Description: Stops all threads and messily empties the task list.
//               This is intended to be called on destruction only.
////////////////////////////////////////////////////////////////////
void AsyncTaskManager::
cleanup() {
  MutexHolder holder(_lock);

  if (task_cat.is_spam()) {
    do_output(task_cat.spam());
    task_cat.spam(false)
      << ": cleanup()\n";
  }

  // Iterate carefully in case the tasks adjust the chain list within
  // cleanup().
  while (!_task_chains.empty()) {
    PT(AsyncTaskChain) chain = _task_chains[_task_chains.size() - 1];
    _task_chains.pop_back();
    chain->do_cleanup();
  }

  // There might be one remaining task, the current task.  Especially
  // if it wasn't running on a thread.
  if (_num_tasks == 1) {
    nassertv(_tasks_by_name.size() == 1);
    TasksByName::const_iterator tbni = _tasks_by_name.begin();
    AsyncTask *task = (*tbni);
    nassertv(task->_state == AsyncTask::S_servicing || 
             task->_state == AsyncTask::S_servicing_removed);
    task->_state = AsyncTask::S_servicing_removed;

  } else {
    // If there isn't exactly one remaining task, there should be
    // none.
#ifndef NDEBUG
    nassertd(_num_tasks == 0 && _tasks_by_name.empty()) {
      task_cat.error()
        << "_num_tasks = " << _num_tasks << " _tasks_by_name = " << _tasks_by_name.size() << "\n";
      TasksByName::const_iterator tbni;
      for (tbni = _tasks_by_name.begin();
           tbni != _tasks_by_name.end();
           ++tbni) {
        task_cat.error()
          << "  " << *(*tbni) << "\n";
      }
    }
#endif  // NDEBUG
  }
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::get_num_task_chains
//       Access: Published
//  Description: Returns the number of different task chains.
////////////////////////////////////////////////////////////////////
int AsyncTaskManager::
get_num_task_chains() const {
  MutexHolder holder(_lock);
  return _task_chains.size();
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::get_task_chain
//       Access: Published
//  Description: Returns the nth task chain.
////////////////////////////////////////////////////////////////////
AsyncTaskChain *AsyncTaskManager::
get_task_chain(int n) const {
  MutexHolder holder(_lock);
  nassertr(n >= 0 && n < (int)_task_chains.size(), NULL);
  return _task_chains[n];
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::make_task_chain
//       Access: Published
//  Description: Creates a new AsyncTaskChain of the indicated name
//               and stores it within the AsyncTaskManager.  If a task
//               chain with this name already exists, returns it
//               instead.
////////////////////////////////////////////////////////////////////
AsyncTaskChain *AsyncTaskManager::
make_task_chain(const string &name) {
  MutexHolder holder(_lock);
  return do_make_task_chain(name);
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::find_task_chain
//       Access: Protected
//  Description: Searches a new AsyncTaskChain of the indicated name
//               and returns it if it exists, or NULL otherwise.
////////////////////////////////////////////////////////////////////
AsyncTaskChain *AsyncTaskManager::
find_task_chain(const string &name) {
  MutexHolder holder(_lock);
  return do_find_task_chain(name);
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::remove_task_chain
//       Access: Protected
//  Description: Removes the AsyncTaskChain of the indicated name.
//               If the chain still has tasks, this will block until
//               all tasks are finished.
//
//               Returns true if successful, or false if the chain did
//               not exist.
////////////////////////////////////////////////////////////////////
bool AsyncTaskManager::
remove_task_chain(const string &name) {
  MutexHolder holder(_lock);

  PT(AsyncTaskChain) chain = new AsyncTaskChain(this, name);
  TaskChains::iterator tci = _task_chains.find(chain);
  if (tci == _task_chains.end()) {
    // No chain.
    return false;
  }

  chain = (*tci);

  while (chain->_num_tasks != 0) {
    // Still has tasks.
    task_cat.info()
      << "Waiting for tasks on chain " << name << " to finish.\n";
    chain->do_wait_for_tasks();
  }

  // Safe to remove.
  chain->do_cleanup();
  _task_chains.erase(tci);
  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::add
//       Access: Published
//  Description: Adds the indicated task to the active queue.  It is
//               an error if the task is already added to this or any
//               other active queue.
////////////////////////////////////////////////////////////////////
void AsyncTaskManager::
add(AsyncTask *task) {
  nassertv(task->is_runnable());

  {
    MutexHolder holder(_lock);
    
    if (task_cat.is_debug()) {
      task_cat.debug()
        << "Adding " << *task << "\n";
    }
    
    if (task->_state == AsyncTask::S_servicing_removed) {
      if (task->_manager == this) {
        // Re-adding a self-removed task; this just means clearing the
        // removed flag.
        task->_state = AsyncTask::S_servicing;
        return;
      }
    }
    
    nassertv(task->_manager == NULL &&
             task->_state == AsyncTask::S_inactive);
    nassertv(!do_has_task(task));
    
    AsyncTaskChain *chain = do_find_task_chain(task->_chain_name);
    if (chain == (AsyncTaskChain *)NULL) {
      task_cat.warning()
        << "Creating implicit AsyncTaskChain " << task->_chain_name
        << " for " << get_type() << " " << get_name() << "\n";
      chain = do_make_task_chain(task->_chain_name);
    }
    chain->do_add(task);
  }

  task->upon_birth();
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::has_task
//       Access: Published
//  Description: Returns true if the indicated task has been added to
//               this AsyncTaskManager, false otherwise.
////////////////////////////////////////////////////////////////////
bool AsyncTaskManager::
has_task(AsyncTask *task) const {
  MutexHolder holder(_lock);

  if (task->_manager != this) {
    nassertr(!do_has_task(task), false);
    return false;
  }

  if (task->_state == AsyncTask::S_servicing_removed) {
    return false;
  }

  // The task might not actually be in the active queue, since it
  // might be being serviced right now.  That's OK.
  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::find_task
//       Access: Published
//  Description: Returns the first task found with the indicated name,
//               or NULL if there is no task with the indicated name.
//
//               If there are multiple tasks with the same name,
//               returns one of them arbitrarily.
////////////////////////////////////////////////////////////////////
AsyncTask *AsyncTaskManager::
find_task(const string &name) const {
  AsyncTask sample_task(name);
  sample_task.local_object();

  TasksByName::const_iterator tbni = _tasks_by_name.lower_bound(&sample_task);
  if (tbni != _tasks_by_name.end() && (*tbni)->get_name() == name) {
    return (*tbni);
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::find_tasks
//       Access: Published
//  Description: Returns the list of tasks found with the indicated
//               name.
////////////////////////////////////////////////////////////////////
AsyncTaskCollection AsyncTaskManager::
find_tasks(const string &name) const {
  AsyncTask sample_task(name);
  sample_task.local_object();

  TasksByName::const_iterator tbni = _tasks_by_name.lower_bound(&sample_task);
  AsyncTaskCollection result;
  while (tbni != _tasks_by_name.end() && (*tbni)->get_name() == name) {
    result.add_task(*tbni);
    ++tbni;
  }

  return result;
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::find_tasks_matching
//       Access: Published
//  Description: Returns the list of tasks found whose name matches
//               the indicated glob pattern, e.g. "my_task_*".
////////////////////////////////////////////////////////////////////
AsyncTaskCollection AsyncTaskManager::
find_tasks_matching(const GlobPattern &pattern) const {
  string prefix = pattern.get_const_prefix();
  AsyncTask sample_task(prefix);
  sample_task.local_object();

  TasksByName::const_iterator tbni = _tasks_by_name.lower_bound(&sample_task);
  AsyncTaskCollection result;
  while (tbni != _tasks_by_name.end() && (*tbni)->get_name().substr(0, prefix.size()) == prefix) {
    AsyncTask *task = (*tbni);
    if (pattern.matches(task->get_name())) {
      result.add_task(task);
    }
    ++tbni;
  }

  return result;
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::remove
//       Access: Published
//  Description: Removes the indicated task from the active queue.
//               Returns true if the task is successfully removed, or
//               false if it wasn't there.
////////////////////////////////////////////////////////////////////
bool AsyncTaskManager::
remove(AsyncTask *task) {
  // We pass this up to the multi-task remove() flavor.  Do we care
  // about the tiny cost of creating an AsyncTaskCollection here?
  // Probably not.
  AsyncTaskCollection tasks;
  tasks.add_task(task);
  return remove(tasks) != 0;
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::remove
//       Access: Published
//  Description: Removes all of the tasks in the AsyncTaskCollection.
//               Returns the number of tasks removed.
////////////////////////////////////////////////////////////////////
int AsyncTaskManager::
remove(const AsyncTaskCollection &tasks) {
  MutexHolder holder(_lock);
  int num_removed = 0;

  int num_tasks = tasks.get_num_tasks();
  int i;
  for (i = 0; i < num_tasks; ++i) {
    PT(AsyncTask) task = tasks.get_task(i);
    
    if (task->_manager != this) {
      // Not a member of this manager, or already removed.
      nassertr(!do_has_task(task), num_removed);
    } else {
      nassertr(task->_chain->_manager == this, num_removed);
      if (task_cat.is_debug()) {
        task_cat.debug()
          << "Removing " << *task << "\n";
      }
      if (task->_chain->do_remove(task)) {
        _lock.release();
        task->upon_death(this, false);
        _lock.lock();
        ++num_removed;
      } else {
        if (task_cat.is_debug()) {
          task_cat.debug()
            << "  (unable to remove " << *task << ")\n";
        }
      }
    }
  }

  return num_removed;
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::wait_for_tasks
//       Access: Published
//  Description: Blocks until the task list is empty.
////////////////////////////////////////////////////////////////////
void AsyncTaskManager::
wait_for_tasks() {
  MutexHolder holder(_lock);

  // Wait for each of our task chains to finish.
  while (_num_tasks > 0) {
    // We iterate through with an index, rather than with an iterator,
    // because it's possible for a task to adjust the task_chain list
    // during its execution.
    for (unsigned int i = 0; i < _task_chains.size(); ++i) {
      AsyncTaskChain *chain = _task_chains[i];
      chain->do_wait_for_tasks();
    }
  }
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::stop_threads
//       Access: Published
//  Description: Stops any threads that are currently running.  If any
//               tasks are still pending and have not yet been picked
//               up by a thread, they will not be serviced unless
//               poll() or start_threads() is later called.
////////////////////////////////////////////////////////////////////
void AsyncTaskManager::
stop_threads() {
  MutexHolder holder(_lock);

  // We iterate through with an index, rather than with an iterator,
  // because it's possible for a task to adjust the task_chain list
  // during its execution.
  for (unsigned int i = 0; i < _task_chains.size(); ++i) {
    AsyncTaskChain *chain = _task_chains[i];
    chain->do_stop_threads();
  }
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::start_threads
//       Access: Published
//  Description: Starts any requested threads to service the tasks on
//               the queue.  This is normally not necessary, since
//               adding a task will start the threads automatically.
////////////////////////////////////////////////////////////////////
void AsyncTaskManager::
start_threads() {
  MutexHolder holder(_lock);

  // We iterate through with an index, rather than with an iterator,
  // because it's possible for a task to adjust the task_chain list
  // during its execution.
  for (unsigned int i = 0; i < _task_chains.size(); ++i) {
    AsyncTaskChain *chain = _task_chains[i];

    chain->do_start_threads();
  }
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::get_tasks
//       Access: Published
//  Description: Returns the set of tasks that are active or sleeping
//               on the task manager, at the time of the call.
////////////////////////////////////////////////////////////////////
AsyncTaskCollection AsyncTaskManager::
get_tasks() const {
  MutexHolder holder(_lock);

  AsyncTaskCollection result;
  TaskChains::const_iterator tci;
  for (tci = _task_chains.begin();
       tci != _task_chains.end();
       ++tci) {
    AsyncTaskChain *chain = (*tci);
    result.add_tasks_from(chain->do_get_active_tasks());
    result.add_tasks_from(chain->do_get_sleeping_tasks());
  }

  return result;
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::get_active_tasks
//       Access: Published
//  Description: Returns the set of tasks that are active (and not
//               sleeping) on the task manager, at the time of the
//               call.
////////////////////////////////////////////////////////////////////
AsyncTaskCollection AsyncTaskManager::
get_active_tasks() const {
  MutexHolder holder(_lock);

  AsyncTaskCollection result;
  TaskChains::const_iterator tci;
  for (tci = _task_chains.begin();
       tci != _task_chains.end();
       ++tci) {
    AsyncTaskChain *chain = (*tci);
    result.add_tasks_from(chain->do_get_active_tasks());
  }

  return result;
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::get_sleeping_tasks
//       Access: Published
//  Description: Returns the set of tasks that are sleeping (and not
//               active) on the task manager, at the time of the
//               call.
////////////////////////////////////////////////////////////////////
AsyncTaskCollection AsyncTaskManager::
get_sleeping_tasks() const {
  MutexHolder holder(_lock);

  AsyncTaskCollection result;
  TaskChains::const_iterator tci;
  for (tci = _task_chains.begin();
       tci != _task_chains.end();
       ++tci) {
    AsyncTaskChain *chain = (*tci);
    result.add_tasks_from(chain->do_get_sleeping_tasks());
  }

  return result;
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::poll
//       Access: Published
//  Description: Runs through all the tasks in the task list, once, if
//               the task manager is running in single-threaded mode
//               (no threads available).  This method does nothing in
//               threaded mode, so it may safely be called in either
//               case.
////////////////////////////////////////////////////////////////////
void AsyncTaskManager::
poll() {
  MutexHolder holder(_lock);

  // We iterate through with an index, rather than with an iterator,
  // because it's possible for a task to adjust the task_chain list
  // during its execution.
  for (unsigned int i = 0; i < _task_chains.size(); ++i) {
    AsyncTaskChain *chain = _task_chains[i];
    chain->do_poll();
  }

  // Just in case the clock was ticked explicitly by one of our
  // polling chains.
  _frame_cvar.signal_all();
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::get_next_wake_time
//       Access: Published
//  Description: Returns the scheduled time (on the manager's clock)
//               of the next sleeping task, on any task chain, to
//               awaken.  Returns -1 if there are no sleeping tasks.
////////////////////////////////////////////////////////////////////
double AsyncTaskManager::
get_next_wake_time() const {
  MutexHolder holder(_lock);

  bool got_any = false;
  double next_wake_time = -1.0;

  TaskChains::const_iterator tci;
  for (tci = _task_chains.begin();
       tci != _task_chains.end();
       ++tci) {
    AsyncTaskChain *chain = (*tci);
    double time = chain->do_get_next_wake_time();
    if (time >= 0.0) {
      if (!got_any) {
        got_any = true;
        next_wake_time = time;
      } else {
        next_wake_time = min(time, next_wake_time);
      }
    }
  }

  return next_wake_time;
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::output
//       Access: Published, Virtual
//  Description: 
////////////////////////////////////////////////////////////////////
void AsyncTaskManager::
output(ostream &out) const {
  MutexHolder holder(_lock);
  do_output(out);
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::write
//       Access: Published, Virtual
//  Description: 
////////////////////////////////////////////////////////////////////
void AsyncTaskManager::
write(ostream &out, int indent_level) const {
  MutexHolder holder(_lock);
  indent(out, indent_level)
    << get_type() << " " << get_name() << "\n";

  TaskChains::const_iterator tci;
  for (tci = _task_chains.begin();
       tci != _task_chains.end();
       ++tci) {
    AsyncTaskChain *chain = (*tci);
    if (chain->_num_tasks != 0) {
      out << "\n";
      chain->do_write(out, indent_level + 2);
    }
  }
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::do_make_task_chain
//       Access: Protected
//  Description: Creates a new AsyncTaskChain of the indicated name
//               and stores it within the AsyncTaskManager.  If a task
//               chain with this name already exists, returns it
//               instead.
//
//               Assumes the lock is held.
////////////////////////////////////////////////////////////////////
AsyncTaskChain *AsyncTaskManager::
do_make_task_chain(const string &name) {
  PT(AsyncTaskChain) chain = new AsyncTaskChain(this, name);

  TaskChains::const_iterator tci = _task_chains.insert(chain).first;
  return (*tci);
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::do_find_task_chain
//       Access: Protected
//  Description: Searches a new AsyncTaskChain of the indicated name
//               and returns it if it exists, or NULL otherwise.
//
//               Assumes the lock is held.
////////////////////////////////////////////////////////////////////
AsyncTaskChain *AsyncTaskManager::
do_find_task_chain(const string &name) {
  PT(AsyncTaskChain) chain = new AsyncTaskChain(this, name);

  TaskChains::const_iterator tci = _task_chains.find(chain);
  if (tci != _task_chains.end()) {
    return (*tci);
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::remove_task_by_name
//       Access: Protected
//  Description: Removes the task from the _tasks_by_name index, if it
//               has a nonempty name.
////////////////////////////////////////////////////////////////////
void AsyncTaskManager::
remove_task_by_name(AsyncTask *task) {
  if (!task->get_name().empty()) {
    // We have to scan linearly through all of the tasks with the same
    // name.
    TasksByName::iterator tbni = _tasks_by_name.lower_bound(task);
    while (tbni != _tasks_by_name.end()) {
      if ((*tbni) == task) {
        _tasks_by_name.erase(tbni);
        return;
      }
      if ((*tbni)->get_name() != task->get_name()) {
        // Too far.
        break;
      }
      
      ++tbni;
    }

    // For some reason, the task wasn't on the index.
    nassertv(false);
  }
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::do_has_task
//       Access: Protected
//  Description: Returns true if the task is on one of the task lists,
//               false if it is not (false may mean that the task is
//               currently being serviced).  Assumes the lock is
//               currently held.
////////////////////////////////////////////////////////////////////
bool AsyncTaskManager::
do_has_task(AsyncTask *task) const {
  TaskChains::const_iterator tci;
  for (tci = _task_chains.begin();
       tci != _task_chains.end();
       ++tci) {
    AsyncTaskChain *chain = (*tci);
    if (chain->do_has_task(task)) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::do_output
//       Access: Protected, Virtual
//  Description: 
////////////////////////////////////////////////////////////////////
void AsyncTaskManager::
do_output(ostream &out) const {
  out << get_type() << " " << get_name()
      << "; " << _num_tasks << " tasks";
}

////////////////////////////////////////////////////////////////////
//     Function: AsyncTaskManager::make_global_ptr
//       Access: Private, Static
//  Description: Called once per application to create the global
//               task manager object.
////////////////////////////////////////////////////////////////////
void AsyncTaskManager::
make_global_ptr() {
  nassertv(_global_ptr == (AsyncTaskManager *)NULL);

  _global_ptr = new AsyncTaskManager("TaskManager");
}
